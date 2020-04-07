#! /bin/bash

head="probe \c"
tail=" {\n\tprintf(\"%s %s\\\n\", thread_indent(0), pp());\n\tprint_ubacktrace();\n}"

while IFS= read -r line
do 
    stap -l "$line.call" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e  $head
        echo "$line.call"
        echo -e $tail

        echo -e  $head
        echo "$line.return"
        echo -e $tail
    fi

    stap -l "$line.inline" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e  $head
        echo "$line.inline"
        echo -e $tail
    fi
done
