set -e
for f in *.s; do
    ../build/vxcc.exe vs2asm --in=$f > $f.asm
done
