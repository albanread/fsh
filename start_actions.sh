#!/bin/bash

# Script to create a new version tag and trigger GitHub Actions release

set -e

echo "=== FasterBASIC Release Tag Creator ==="
echo ""

# Get the latest tag
LATEST_TAG=$(git tag --list "v*" | sort -V | tail -1)

if [ -z "$LATEST_TAG" ]; then
    echo "No existing tags found. Starting with v1.0.0"
    NEW_TAG="v1.0.0"
else
    echo "Latest tag: $LATEST_TAG"

    # Remove 'v' prefix and split version parts
    VERSION=${LATEST_TAG#v}
    MAJOR=$(echo $VERSION | cut -d. -f1)
    MINOR=$(echo $VERSION | cut -d. -f2)
    PATCH=$(echo $VERSION | cut -d. -f3)

    echo ""
    echo "Select version bump type:"
    echo "1) Major (currently $MAJOR) - Breaking changes"
    echo "2) Minor (currently $MINOR) - New features"
    echo "3) Patch (currently $PATCH) - Bug fixes"
    echo ""
    read -p "Enter choice [1-3]: " CHOICE

    case $CHOICE in
        1)
            MAJOR=$((MAJOR + 1))
            MINOR=0
            PATCH=0
            ;;
        2)
            MINOR=$((MINOR + 1))
            PATCH=0
            ;;
        3)
            PATCH=$((PATCH + 1))
            ;;
        *)
            echo "Invalid choice. Exiting."
            exit 1
            ;;
    esac

    NEW_TAG="v${MAJOR}.${MINOR}.${PATCH}"
fi

echo ""
echo "New tag will be: $NEW_TAG"
echo ""
read -p "Enter release description (optional): " DESCRIPTION

if [ -z "$DESCRIPTION" ]; then
    DESCRIPTION="Release $NEW_TAG"
fi

echo ""
echo "Creating tag: $NEW_TAG"
echo "Description: $DESCRIPTION"
echo ""
read -p "Proceed? (y/n): " CONFIRM

if [ "$CONFIRM" != "y" ] && [ "$CONFIRM" != "Y" ]; then
    echo "Cancelled."
    exit 0
fi

# Create annotated tag
git tag -a "$NEW_TAG" -m "$DESCRIPTION"

echo ""
echo "Tag created locally. Pushing to GitHub..."

# Push the tag
git push origin "$NEW_TAG"

echo ""
echo "✅ Tag $NEW_TAG pushed successfully!"
echo ""
echo "GitHub Actions will now:"
echo "  1. Build binaries for macOS and Linux"
echo "  2. Create a new release at: https://github.com/albanread/fsh/releases/tag/$NEW_TAG"
echo "  3. Upload the built binaries as release assets"
echo ""
echo "Check the progress at: https://github.com/albanread/fsh/actions"
