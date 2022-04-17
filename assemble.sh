if [ $# -ne 2 ]; then
	echo "Expects <input file> <output file>"
	exit 1
fi

TMPFILE=$(mktemp)
cpp -P $1 $TMPFILE
./asm -e $TMPFILE $2
rm $TMPFILE
