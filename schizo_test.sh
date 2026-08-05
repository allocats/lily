#!/bin/bash

> ./tests/schizo.lily

echo "module lily;" >> ./tests/schizo.lily
echo "" >> ./tests/schizo.lily

for ((i = 0; i < 8192; i++)); do
    echo "struct Vec$i { x:i32; y: i32; }" >> ./tests/schizo.lily
    echo "" >> ./tests/schizo.lily
done
