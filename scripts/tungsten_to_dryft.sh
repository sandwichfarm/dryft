#!/bin/bash

# Script to update remaining tungsten references to dryft
# This handles case-sensitive replacements

echo "=== Updating remaining tungsten references to dryft ==="

# Function to update file content
update_file() {
    local file="$1"
    local temp_file="${file}.tmp"
    
    # Skip binary files and already processed files
    if file -b "$file" | grep -q "text"; then
        # Replace Tungsten with dryft (preserving case in some contexts)
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
            -e 's/namespace dryft/namespace dryft/g' \
            -e 's/dryft::/dryft::/g' \
            -e 's/DRYFT_/DRYFT_/g' \
            -e 's/tungsten\//dryft\//g' \
            -e 's/\/tungsten\//\/dryft\//g' \
            -e 's/github.com\/sandwichfarm\/dryft/github.com\/sandwichfarm\/dryft/g' \
            -e 's/#dryft:/#dryft:/g' \
            -e 's/tungsten\.dev/dryft.dev/g' \
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

# Find all text files with tungsten references
echo "Finding files with tungsten references..."
files=$(grep -rl -i "tungsten" --include="*.cc" --include="*.h" --include="*.gni" --include="*.gn" --include="*.md" --include="*.js" --include="*.html" --include="*.css" --include="*.py" --include="*.sh" --include="*.json" . 2>/dev/null | grep -v ".git" | sort -u)

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