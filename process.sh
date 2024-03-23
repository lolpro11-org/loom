#!/bin/bash

# Check if gtfs2graph is installed
if ! command -v gtfs2graph &> /dev/null; then
    echo "gtfs2graph is not installed or not in the PATH. Aborting."
    exit 1
fi

# Directory containing the .zip files
input_dir="/home/lolpro11/gtfs-schema/gtfs"
output_dir="/home/lolpro11/loom/renderings"
# Check if the input directory exists
if [ ! -d "$input_dir" ]; then
    echo "Input directory does not exist: $input_dir"
    exit 1
fi

# Iterate over each .zip file in the directory
for zip_file in "$input_dir"/*.zip; do
    if [ -f "$zip_file" ]; then
        # Extract filename without extension
        filename=$(basename -- "$zip_file")
        filename_no_ext="${filename%.*}"

        # Execute the gtfs2graph command with the pipeline
        gtfs2graph -m tram "$zip_file" | topo | loom | octi | transitmap > "${output_dir}/${filename_no_ext}.svg"

        echo "Processed: $zip_file"
    fi
done

