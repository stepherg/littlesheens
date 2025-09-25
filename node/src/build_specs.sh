#!/bin/bash
mkdir -p specs/
for filepath in spec_source/*.yaml; do
   filename=$(basename "$filepath" .yaml)
   yaml2json < "$filepath" | jq . > "specs/${filename}.json.tmp"
   {
      echo "// Auto-generated from spec_source/${filename}.yaml -- DO NOT EDIT DIRECTLY";
      echo "module.exports = "
      cat "specs/${filename}.json.tmp"
      echo ";";
   } > "specs/${filename}.js"
   rm -f "specs/${filename}.json.tmp"
done
