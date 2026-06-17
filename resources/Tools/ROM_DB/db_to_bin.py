import os
import json
import sqlite3
import struct

# Estrutura do registro binário (128 bytes no total)
# Struct format: 32s (md5) 64s (nome) I (tamanho) 16s (bank_model) c (letra) 11s (padding)
STRUCT_FORMAT = "<32s64sI16sc11s"

def get_initial_letter(name):
    if not name:
        return b'A'
    name_strip = name.strip()
    if not name_strip:
        return b'A'
    first_char = name_strip[0].upper()
    if 'A' <= first_char <= 'Z':
        return first_char.encode('ascii')
    elif '0' <= first_char <= '9':
        return b'#'
    else:
        return b'#'

def convert():
    records = []
    db_path = os.path.join(os.path.dirname(__file__), "roms_database.db")
    json_path = os.path.join(os.path.dirname(__file__), "roms.json")
    out_path = os.path.join(os.path.dirname(__file__), "roms.bin")

    # 1. Tentar ler do SQLite
    use_sqlite = False
    if os.path.exists(db_path):
        try:
            print(f"[INFO] Conectando ao banco SQLite: {db_path}")
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            # Verifica as colunas da tabela roms
            cursor.execute("PRAGMA table_info(roms)")
            columns = [col[1] for col in cursor.fetchall()]
            print(f"[INFO] Colunas encontradas na tabela 'roms': {columns}")

            # Busca todos os dados ordenados por nome_principal
            query = "SELECT md5, nome_principal, tamanho_kb, bank_model FROM roms ORDER BY nome_principal"
            cursor.execute(query)
            rows = cursor.fetchall()
            for row in rows:
                md5, nome, tam_kb, bank = row
                records.append({
                    "md5": md5 or "",
                    "nome": nome or "Unknown Game",
                    "tamanho_bytes": int(tam_kb or 0) * 1024,
                    "bank_model": bank or "Standard"
                })
            conn.close()
            use_sqlite = True
            print(f"[INFO] Sucesso: {len(records)} jogos lidos do SQLite.")
        except Exception as e:
            print(f"[WARNING] Erro ao ler SQLite: {e}. Usando fallback para roms.json...")

    # 2. Se falhar ou não usar SQLite, lê do JSON
    if not use_sqlite:
        if os.path.exists(json_path):
            print(f"[INFO] Lendo arquivo JSON: {json_path}")
            try:
                with open(json_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                for item in data:
                    md5 = item.get("md5", "")
                    nome = item.get("nome_principal", "Unknown Game")
                    tam_kb = item.get("tamanho_kb", 0)
                    bank = item.get("bank_model", "Standard")
                    records.append({
                        "md5": md5,
                        "nome": nome,
                        "tamanho_bytes": int(tam_kb or 0) * 1024,
                        "bank_model": bank
                    })
                print(f"[INFO] Sucesso: {len(records)} jogos lidos do JSON.")
            except Exception as e:
                print(f"[ERROR] Falha ao ler JSON: {e}")
                return
        else:
            print("[ERROR] Nem roms_database.db nem roms.json foram encontrados!")
            return

    # 3. Ordenar registros por nome
    records.sort(key=lambda x: x["nome"].lower())

    # 4. Escrever o arquivo binário
    print(f"[INFO] Gerando arquivo binário indexado: {out_path}")
    count_written = 0
    with open(out_path, "wb") as bin_file:
        for rec in records:
            md5_str = rec["md5"].strip()
            nome_str = rec["nome"].strip()
            tam_bytes = rec["tamanho_bytes"]
            bank_str = rec["bank_model"].strip()

            # Mapeia a letra
            letra_byte = get_initial_letter(nome_str)

            # Codificação e padding de strings para o struct
            md5_bytes = md5_str.encode('ascii')[:32].ljust(32, b'\x00')
            nome_bytes = nome_str.encode('utf-8', errors='ignore')[:63].ljust(64, b'\x00')
            bank_bytes = bank_str.encode('ascii', errors='ignore')[:15].ljust(16, b'\x00')
            padding_bytes = b'\x00' * 11

            # Empacota registro de 128 bytes
            record_packed = struct.pack(
                STRUCT_FORMAT,
                md5_bytes,
                nome_bytes,
                tam_bytes,
                bank_bytes,
                letra_byte,
                padding_bytes
            )
            bin_file.write(record_packed)
            count_written += 1

    print(f"[SUCCESS] Arquivo '{out_path}' gerado com sucesso!")
    print(f"[SUCCESS] Total de registros gravados: {count_written} (Tamanho total: {count_written * 128} bytes)")

if __name__ == "__main__":
    convert()
