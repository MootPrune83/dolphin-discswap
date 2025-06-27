import os
import glob
from PIL import Image

# 1. Gather source images from .\input
INPUT_DIR = r'.\input'
# adjust/extensions as needed
EXTENSIONS = ('*.jpg', '*.jpeg', '*.png', '*.bmp', '*.gif')

source_paths = []
for ext in EXTENSIONS:
    source_paths.extend(glob.glob(os.path.join(INPUT_DIR, ext)))
source_paths.sort()

if not source_paths:
    print(f"No images found in {INPUT_DIR}.")
    exit(1)

print("Available source images:")
for idx, path in enumerate(source_paths, start=1):
    print(f"{idx:2d}. {os.path.basename(path)}")
print()

# 2. Example Target List
targets = [
    ("LEGO Star Wars - The Complete Saga", "swarscomplete.jpg"),
    ("Mario Kart Wii",                "mariokartwii.jpg"),
    ("Mario Party 9",                 "marioparty9.jpg"),
    ("Mario Sports Mix",              "mariosportsmix.jpg"),
    ("New Super Mario Bros Wii",      "newsupermariobroswii.jpg"),
    ("Super Mario Galaxy 2",          "supermariogalaxy2.jpg"),
    ("Super Smash Bros Brawl",        "supersmashbrosbrawl.jpg"),
    ("Wii Party",                     "wiiparty.jpg"),
    ("Wii Sports + Wii Sports Resort","wiisportsresort.jpg"),
]

print("Target slots:")
for idx, (label, filename) in enumerate(targets, start=1):
    print(f"{idx:2d}. {label:20s} → {filename}")
print()

# 3. Prompt user for selections
def ask_index(prompt, max_index):
    while True:
        try:
            choice = int(input(prompt))
            if 1 <= choice <= max_index:
                return choice - 1
        except ValueError:
            pass
        print(f"Please enter a number between 1 and {max_index}.")

src_idx = ask_index("Select source image number: ", len(source_paths))
tgt_idx = ask_index("Select target slot number:  ", len(targets))

src_path = source_paths[src_idx]
tgt_label, tgt_filename = targets[tgt_idx]

# 4. Process and save
try:
    with Image.open(src_path) as img:
        resized = img.resize((180, 242), Image.LANCZOS)
        resized.save(tgt_filename, 'JPEG')
    print(f"Saved {src_path!r} → {tgt_filename!r} (180×242).")
except Exception as e:
    print("Error processing image:", e)
