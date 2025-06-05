#!/bin/bash
mkdir -p specs/
for filepath in spec_source/*.yaml; do
   filename=$(basename "$filepath" .yaml)
   #echo ${filename}
	cat ${filepath} | yaml2json | jq . > specs/${filename}.js
done
