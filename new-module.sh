#!/bin/bash
# Script to create a new KPM module from template

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULES_DIR="$SCRIPT_DIR/modules"
TEMPLATE_DIR="$MODULES_DIR/template"

# Check if module name is provided
if [ -z "$1" ]; then
    echo -e "${RED}Error: Module name is required${NC}"
    echo ""
    echo "Usage: $0 <module-name> [author] [description]"
    echo ""
    echo "Examples:"
    echo "  $0 my-module"
    echo "  $0 my-module \"Your Name\" \"My awesome module\""
    exit 1
fi

MODULE_NAME="$1"
AUTHOR="${2:-Your Name}"
DESCRIPTION="${3:-KPM Module - $MODULE_NAME}"

MODULE_DIR="$MODULES_DIR/$MODULE_NAME"

# Check if module already exists
if [ -d "$MODULE_DIR" ]; then
    echo -e "${RED}Error: Module '$MODULE_NAME' already exists${NC}"
    exit 1
fi

# Check if template exists
if [ ! -d "$TEMPLATE_DIR" ]; then
    echo -e "${RED}Error: Template directory not found: $TEMPLATE_DIR${NC}"
    exit 1
fi

echo -e "${GREEN}Creating new KPM module: $MODULE_NAME${NC}"
echo ""

# Create module directory
mkdir -p "$MODULE_DIR"

# Copy template files
cp "$TEMPLATE_DIR/module.c" "$MODULE_DIR/module.c"
cp "$TEMPLATE_DIR/module.lds" "$MODULE_DIR/module.lds"

# Replace placeholders in module.c
sed -i.bak "s/kpm-template/kpm-$MODULE_NAME/g" "$MODULE_DIR/module.c"
sed -i.bak "s/Your Name/$AUTHOR/g" "$MODULE_DIR/module.c"
sed -i.bak "s/KPM Template Module/$DESCRIPTION/g" "$MODULE_DIR/module.c"
sed -i.bak "s/template_/$(echo $MODULE_NAME | tr '-' '_')_/g" "$MODULE_DIR/module.c"
sed -i.bak "s/kpm template/kpm $MODULE_NAME/g" "$MODULE_DIR/module.c"

# Remove backup files
rm -f "$MODULE_DIR"/*.bak

echo -e "${GREEN}✅ Module created successfully!${NC}"
echo ""
echo "Module location: $MODULE_DIR"
echo ""
echo "Next steps:"
echo "  1. Edit $MODULE_DIR/module.c to implement your module"
echo "  2. Run: ./build.sh"
echo "  3. Or build specific module: cmake --build build --target $MODULE_NAME"
echo ""
echo -e "${YELLOW}Files created:${NC}"
ls -la "$MODULE_DIR"


