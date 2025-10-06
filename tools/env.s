#!/bin/bash
# Environment setup for DolHook development

# Detect devkitPro installation
if [ -z "$DEVKITPRO" ]; then
    if [ -d "/opt/devkitpro" ]; then
        export DEVKITPRO=/opt/devkitpro
    elif [ -d "$HOME/devkitpro" ]; then
        export DEVKITPRO=$HOME/devkitpro
    else
        echo "Error: devkitPro not found!"
        echo "Please install devkitPro from https://devkitpro.org/wiki/Getting_Started"
        return 1
    fi
fi

export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH

# Verify installation
if ! command -v powerpc-eabi-gcc &> /dev/null; then
    echo "Error: powerpc-eabi-gcc not found in PATH"
    echo "DEVKITPPC=$DEVKITPPC"
    return 1
fi

echo "DolHook environment configured:"
echo "  DEVKITPRO=$DEVKITPRO"
echo "  DEVKITPPC=$DEVKITPPC"
echo "  Compiler: $(powerpc-eabi-gcc --version | head -n1)"

# Optional: Set aliases
alias build-runtime='make runtime'
alias build-patcher='make patcher'
alias build-all='make all'
alias patch-iso='./patchiso'

echo ""
echo "Ready to build! Try:"
echo "  make all       - Build everything"
echo "  make runtime   - Build PPC payload only"
echo "  make patcher   - Build patcher only"