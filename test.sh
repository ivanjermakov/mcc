#!/usr/bin/env sh

passed=0
failed=0

for f in test/*; do
    name=$(basename $f)
    name=${name%.*}
    rm -f build/$name.o
	build/mcc test/$name.c build/$name.o
    rm -f build/$name
	ld /usr/lib64/crt1.o /usr/lib64/crti.o /usr/lib64/crtn.o build/$name.o -lc -dynamic-linker /usr/lib64/ld-linux-x86-64.so.2 -o build/$name

    expected=$(cat test/$name.c | grep -m 1 '^// [0-9]' | awk '{print $2}')
	build/$name
    actual=$?
    if [[ $expected == $actual ]]; then
        echo -e "\e[32mok\e[0m $name"
        ((passed++))
    else
        echo -e "\e[31mfail\e[0m $name: expected $expected, got $actual"
        ((failed++))
    fi
done
echo -ne "$passed \e[32mok\e[0m"
if (( failed != 0 )); then
    echo -e ", $failed \e[31mfail\e[0m"
    exit 1
fi
echo
