# A tour of the BusyBox shell running on Tenok.
#
#     busybox sh /rom_data/demo.sh
#
# Plain POSIX shell: Tenok has no fork(), so there are no pipes and no
# command substitution. Everything below works without them.

passed=0
failed=0

check() {
        if [ "$1" -eq 0 ]; then
                echo "  ok    $2"
                passed=$((passed + 1))
        else
                echo "  FAIL  $2"
                failed=$((failed + 1))
        fi
}

echo "=== the system ==="
uname -a
echo "shell started in $PWD"

echo
echo "=== what is mounted ==="
ls /
echo "devices:"
ls /dev

echo
echo "=== files ==="
work=/demo
rm -r $work 2> /dev/null

mkdir $work
check $? "mkdir $work"

echo "the quick brown fox" > $work/a.txt
check $? "write a.txt"

echo "jumps over the lazy dog" >> $work/a.txt
check $? "append to a.txt"

echo "contents:"
cat $work/a.txt
wc $work/a.txt

read first < $work/a.txt
echo "first line read back: $first"

mv $work/a.txt $work/b.txt
check $? "rename to b.txt"

ls -l $work

if [ -f $work/b.txt ] && [ ! -f $work/a.txt ]; then
        check 0 "the rename took effect"
else
        check 1 "the rename took effect"
fi

rm -r $work
check $? "remove $work"

echo
echo "=== arithmetic and loops ==="
i=1
total=0
while [ $i -le 5 ]; do
        total=$((total + i * i))
        echo "  $i squared, running total is $total"
        i=$((i + 1))
done

for name in poem.txt more/animal.txt; do
        echo "  lines in $name:"
        wc -l /rom_data/$name
done

echo
echo "=== errors are reported ==="
cat /rom_data/does-not-exist
echo "  exit status was $?"

echo
echo "=== $passed passed, $failed failed ==="
