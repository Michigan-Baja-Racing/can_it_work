import os
import subprocess

DBC_FILE = "dbc/MBR_DBC.dbc"
SCRIPTS = "scripts"
SRC_DIR = "src"
INCLUDE_DIR = "include"
FILENAME_BASE = "mbr_dbc"

os.makedirs(SRC_DIR, exist_ok=True)
os.makedirs(INCLUDE_DIR, exist_ok=True)

subprocess.run(
    [
        "cantools",
        "generate_c_source",
        DBC_FILE,
        "--database-name",
        FILENAME_BASE,
        "-o",
        SCRIPTS,
    ]
)

os.replace(
    os.path.join(SCRIPTS, FILENAME_BASE + ".h"),
    os.path.join(INCLUDE_DIR, FILENAME_BASE + ".h"),
)

os.replace(
    os.path.join(SCRIPTS, FILENAME_BASE + ".c"),
    os.path.join(SRC_DIR, FILENAME_BASE + ".c"),
)
