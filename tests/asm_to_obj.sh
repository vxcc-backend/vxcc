for f in *.asm; do
    nasm $f -o $f.o
done
