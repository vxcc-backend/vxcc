set -e
for f in *.c3; do
    ../../build/c3c* compile-only $f --use-stdlib=no --backend=vxcc --emit-asm > $f.s
done
