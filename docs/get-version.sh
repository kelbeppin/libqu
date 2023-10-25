#!/bin/sh

# First argument is the path to the libqu.h header file.
HEADER_FILE="$1"

# Meh.
if [ -z "${HEADER_FILE}" ]; then
    echo "unknown"
    exit 0
fi

# Extract the line which holds QU_VERSION.
# Note: this will fail miserably if there is more than one line
# that holds such substring.
VERSION_LINE=$(grep 'QU_VERSION' "${HEADER_FILE}")

# Print third column (after '#define' and 'QU_VERSION'),
# removing quotes.
VERSION=$(echo ${VERSION_LINE} | awk '{ gsub(/"/, "", $3); print $3 }')

# Add git commit hash if version ends in '-dev'.
case ${VERSION} in
    *-dev)
        VERSION="${VERSION} ($(git rev-parse --short HEAD))"
esac

# Output the result.
echo "$VERSION"
