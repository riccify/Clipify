#!/bin/sh
set -eu

directory="${XDG_DATA_HOME:-$HOME/.local/share}/Clipify/Clipify/ocr/tessdata_best"
mkdir -p "$directory"
temporary=$(mktemp -d "$directory/.download.XXXXXXXX")
trap 'rm -rf -- "$temporary"' EXIT

base=https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/4.1.0
for language in deu eng; do
    curl --fail --silent --show-error --location "$base/$language.traineddata" \
        --output "$temporary/$language.traineddata"
done

(
    cd "$temporary"
    sha256sum -c <<'CHECKSUMS'
8407331d6aa0229dc927685c01a7938fc5a641d1a9524f74838cdac599f0d06e  deu.traineddata
8280aed0782fe27257a68ea10fe7ef324ca0f8d85bd2fd145d1c2b560bcb66ba  eng.traineddata
CHECKSUMS
)
mv -- "$temporary/deu.traineddata" "$temporary/eng.traineddata" "$directory/"
