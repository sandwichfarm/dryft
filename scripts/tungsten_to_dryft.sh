#!/bin/bash

# Script to update remaining dryft references to dryft
# This handles case-sensitive replacements

echo "=== Updating remaining dryft references to dryft ==="

# Function to update file content
update_file() {
    local file="$1"
    local temp_file="${file}.tmp"
    
    # Skip binary files and already processed files
    if file -b "$file" | grep -q "text"; then
        # Replace dryft with dryft (comprehensive patterns)
        sed -e 's/dryft Authors/dryft Authors/g' \
            -e 's/dryft browser/dryft browser/g' \
            -e 's/dryft browser/dryft browser/g' \
            -e 's/dryft Nostr/dryft Nostr/g' \
            -e 's/dryft-specific/dryft-specific/g' \
            -e 's/dryft macOS/dryft macOS/g' \
            -e 's/dryft root/dryft root/g' \
            -e 's/dryft source/dryft source/g' \
            -e 's/dryft development/dryft development/g' \
            -e 's/dryft community/dryft community/g' \
            -e 's/dryft features/dryft features/g' \
            -e 's/dryft project/dryft project/g' \
            -e 's/dryft setup/dryft setup/g' \
            -e 's/dryft instances/dryft instances/g' \
            -e 's/dryft is a fork/dryft is a fork/g' \
            -e 's/dryft extends/dryft extends/g' \
            -e 's/dryft to natively/dryft to natively/g' \
            -e 's/dryft\/Nostr/dryft\/Nostr/g' \
            -e 's/Building dryft/Building dryft/g' \
            -e 's/Build dryft/Build dryft/g' \
            -e 's/dryft inherits/dryft inherits/g' \
            -e "s/dryft's/dryft's/g" \
            -e 's/namespace dryft/namespace dryft/g' \
            -e 's/dryft::/dryft::/g' \
            -e 's/DRYFT_/DRYFT_/g' \
            -e 's/dryft\//dryft\//g' \
            -e 's/\/dryft\//\/dryft\//g' \
            -e 's/\.dryft/.dryft/g' \
            -e 's/com\.dryft/com.dryft/g' \
            -e 's/github\.com\/.*\/dryft/github.com\/sandwichfarm\/dryft/g' \
            -e 's/#dryft:/#dryft:/g' \
            -e 's/dryft\.dev/dryft.dev/g' \
            -e 's/cd dryft/cd dryft/g' \
            -e 's/clone.*dryft\.git/clone https:\/\/github.com\/sandwichfarm\/dryft.git/g' \
            "$file" > "$temp_file"
        
        # Only replace if file changed
        if ! cmp -s "$file" "$temp_file"; then
            mv "$temp_file" "$file"
            echo "Updated: $file"
        else
            rm "$temp_file"
        fi
    fi
}

# Find all text files with dryft references
echo "Finding files with dryft references..."
files=$(grep -rl -i "dryft" --include="*.cc" --include="*.h" --include="*.gni" --include="*.gn" --include="*.md" --include="*.js" --include="*.html" --include="*.css" --include="*.py" --include="*.sh" --include="*.json" . 2>/dev/null | grep -v ".git" | sort -u)

total=$(echo "$files" | wc -l)
current=0

echo "Found $total files to process"

# Update each file
for file in $files; do
    current=$((current + 1))
    echo "Processing ($current/$total): $file"
    update_file "$file"
done

echo "=== Update complete ==="
echo "Please review changes with: git diff"
echo "Stage changes with: git add -u"