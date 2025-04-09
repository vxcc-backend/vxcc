set -e

if [ -f ../../build/c3c* ]; then
    echo "# frontend test"
    ./c3_to_ir.sh
fi

echo "# opt & codegen test"
./ir_to_asm.sh

if hash nasm 2>/dev/null; then
    echo "# validate codegen test"
    ./asm_to_obj.sh
fi
