import os
import json
import sqlite3
import struct
from collections import defaultdict

# ============================================================
# FORMATO DO BANCO
# ============================================================

# Registro físico fixo (128 bytes: 32 + 64 + 4 + 16 + 1 + 1 + 10)
RECORD_STRUCT = struct.Struct("<32s64sI16scB10s")

# Header:
#
# magic       -> 4 bytes
# version     -> 4 bytes
#
# 27 entradas:
#   start      -> uint32
#   count      -> uint32
#
# Total:
# 4 + 4 + (27 * 8) = 224 bytes

HEADER_STRUCT = struct.Struct("<4sI" + ("II" * 27))

MAGIC = b"ROM1"
VERSION = 1

LETTERS = ['#'] + [chr(ord('A') + i) for i in range(26)]


# ============================================================
# UTILIDADES
# ============================================================

def get_initial_letter(name):
    if not name:
        return '#'

    name = name.strip()

    if not name:
        return '#'

    c = name[0].upper()

    if 'A' <= c <= 'Z':
        return c

    return '#'


# ============================================================
# LEITURA DOS DADOS
# ============================================================

def load_records():
    records = []

    db_path = os.path.join(os.path.dirname(__file__), "roms_database.db")
    json_path = os.path.join(os.path.dirname(__file__), "roms.json")

    use_sqlite = False

    # --------------------------------------------------------
    # SQLITE
    # --------------------------------------------------------
    if os.path.exists(db_path):
        try:
            print(f"[INFO] Abrindo SQLite: {db_path}")
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()

            query = """
            SELECT
                md5,
                nome_principal,
                tamanho_kb,
                bank_model
            FROM roms
            """
            cursor.execute(query)

            for row in cursor.fetchall():
                md5, nome, tam_kb, bank = row
                nome = nome or "Unknown Game"

                records.append({
                    "md5": md5 or "",
                    "nome": nome,
                    "tamanho_bytes": int(tam_kb or 0) * 1024,
                    "bank_model": bank or "Standard",
                    "letter": get_initial_letter(nome)
                })

            conn.close()
            use_sqlite = True
            print(f"[INFO] {len(records)} jogos carregados do SQLite.")
        except Exception as e:
            print(f"[WARNING] SQLite falhou: {e}")

    # --------------------------------------------------------
    # JSON FALLBACK
    # --------------------------------------------------------
    if not use_sqlite:
        if not os.path.exists(json_path):
            print("[ERROR] Nenhuma base encontrada.")
            return []

        print(f"[INFO] Abrindo JSON: {json_path}")
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for item in data:
            nome = item.get("nome_principal", "Unknown Game")
            records.append({
                "md5": item.get("md5", ""),
                "nome": nome,
                "tamanho_bytes": int(item.get("tamanho_kb", 0)) * 1024,
                "bank_model": item.get("bank_model", "Standard"),
                "letter": get_initial_letter(nome)
            })

        print(f"[INFO] {len(records)} jogos carregados do JSON.")

    return records


# ============================================================
# GERAÇÃO DO BINÁRIO
# ============================================================

def generate_bin():
    records = load_records()
    if not records:
        return

    # --------------------------------------------------------
    # ORGANIZA POR LETRA
    # --------------------------------------------------------
    grouped = defaultdict(list)
    for rec in records:
        grouped[rec["letter"]].append(rec)

    # Ordena internamente por nome
    for letter in grouped:
        grouped[letter].sort(key=lambda x: x["nome"].lower())

    # --------------------------------------------------------
    # PREPARA HEADER
    # --------------------------------------------------------
    letter_index = {}
    current_record_index = 0

    for letter in LETTERS:
        items = grouped.get(letter, [])
        start = current_record_index
        count = len(items)
        letter_index[letter] = (start, count)
        current_record_index += count

    # --------------------------------------------------------
    # ESCREVE BINÁRIO
    # --------------------------------------------------------
    out_path = os.path.join(os.path.dirname(__file__), "roms.bin")

    print(f"[INFO] Gerando: {out_path}")

    with open(out_path, "wb") as f:
        # ====================================================
        # HEADER
        # ====================================================
        header_values = [
            MAGIC,
            VERSION
        ]

        for letter in LETTERS:
            start, count = letter_index[letter]
            header_values.append(start)
            header_values.append(count)

        f.write(HEADER_STRUCT.pack(*header_values))

        # ====================================================
        # BLOCOS FÍSICOS
        # ====================================================
        total_written = 0

        for letter in LETTERS:
            items = grouped.get(letter, [])
            for rec in items:
                md5_bytes = (
                    rec["md5"]
                    .encode("ascii", errors="ignore")[:32]
                    .ljust(32, b'\x00')
                )

                nome_bytes = (
                    rec["nome"]
                    .encode("utf-8", errors="ignore")[:63]
                    .ljust(64, b'\x00')
                )

                bank_bytes = (
                    rec["bank_model"]
                    .encode("ascii", errors="ignore")[:15]
                    .ljust(16, b'\x00')
                )

                record = RECORD_STRUCT.pack(
                    md5_bytes,
                    nome_bytes,
                    rec["tamanho_bytes"],
                    bank_bytes,
                    letter.encode("ascii"),
                    0,  # sd field = 0 (not present initially)
                    b'\x00' * 10
                )

                f.write(record)
                total_written += 1

    print("[SUCCESS] Banco gerado.")
    print(f"[SUCCESS] Registros: {total_written}")
    print(f"[SUCCESS] Header size: {HEADER_STRUCT.size} bytes")
    print(f"[SUCCESS] Record size: {RECORD_STRUCT.size} bytes")
    print(f"[SUCCESS] Tamanho total: {HEADER_STRUCT.size + total_written * RECORD_STRUCT.size} bytes")


# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":
    generate_bin()
