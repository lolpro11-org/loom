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
# Function to process a single ZIP file
process_zip() {
    local zip_file="$1"
    if [ -f "$zip_file" ]; then
        # Extract filename without extension
        filename=$(basename -- "$zip_file")
        filename_no_ext="${filename%.*}"

        # Execute the gtfs2graph command with the pipeline
        # gtfs2graph "$zip_file" | topo | loom | octi | transitmap > "${output_dir}/${filename_no_ext}.svg"
        gtfs2graph "$zip_file" | topo | loom | octi > "geojson/${filename_no_ext}.geojson" && echo "Processed: $zip_file"
    fi
}

# Export the function to make it available to parallel
export -f process_zip

# Iterate over each .zip file in the directory and process them with limited concurrency
for zip_file in "$input_dir"/*.zip; do
    if [ -f "$zip_file" ]; then
        process_zip "$zip_file" &
        # Count the number of background processes
        num_processes=$(jobs -p | wc -l)
        # Wait until the number of background processes is less than 64
        while [ "$num_processes" -ge 32 ]; do
            sleep 1
            num_processes=$(jobs -p | wc -l)
        done
    fi
done

# Wait for all background processes to finish
wait
