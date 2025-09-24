set -euxo pipefail
mkdir -p dist
gcc -o dist/main src/main.c
./dist/main