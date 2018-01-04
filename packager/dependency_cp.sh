#!/bin/bash 
# Author : Hemanth.HM
# Email : hemanth[dot]hm[at]gmail[dot]com
# License : GNU GPLv3
#
# watkinspv - modified for only copying patterns and use also with otool

function useage()
{
    cat << EOU
Useage: bash $0 <path to the binary> <path to copy the dependencies> <matching pattern to copy> <matching pattern to not copy> <: delimited library paths if not found> <use ldd==0 otool==1 depends==2>
EOU
exit 1
}

#Validate the inputs
[[ $# < 5 ]] && useage

#Check if the paths are vaild
[[ ! -e $1 ]] && echo "Not a vaild input $1" && exit 1 
[[ -d $2 ]] || echo "No such directory $2 creating..."&& mkdir -p "$2"

# change delimiters in library search path to newline
search=$(echo "$5" | tr ':' '\n')
echo; echo "Search path"; echo $search

#set -e # hard errors
IGNORE_CASE=

echo; echo "Collecting the shared library dependencies for $1..."
if [ "$6" = 1 ]; then
    # get the directory this script is located in
    SOURCE="${BASH_SOURCE[0]}"
    while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
        DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
        SOURCE="$(readlink "$SOURCE")"
        [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" 
    done
    DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

    # Get the library dependencies using python script that calls otool
    deps=$($DIR/otool_recursive.py $1)
elif [ "$6" = 2 ]; then
    # Get the library dependencies using dependency walker (installed with chocolatey)
    #[[ ":$PATH:" != *":/c/ProgramData/chocolatey/bin:"* ]] && export PATH="${PATH}:/c/ProgramData/chocolatey/bin"
    cmd.exe /C '"depends.exe /c /f:1 /oc:depends.csv '"$(echo $1 | sed 's/\//\\/g')"'"'
    # xxx - blocking call to depends??? batch files are dead, long live batch files!
    #start //wait cmd.exe /C '"depends.exe /c /f:1 /oc:depends.csv '"$(echo $1 | sed 's/\//\\/g')"'"'
    echo 'Waiting for dependency walker...'; sleep 60
    deps=$(tail -n +2 depends.csv | awk -F',' '{gsub(/"/, "", $2); print $2}' | tr '[A-Z]' '[a-z]')
    deps=$(cygpath $deps); rm depends.csv
    IGNORE_CASE=i
else
    # Get the library dependencies using ldd
    deps=$(ldd $1 | awk 'BEGIN{ORS="\n"}\
        $1~/^\//{print $1} $3~/^\//{print $3} $1!~/^\//&&$3!~/^\//{print $1}' | sed 's/,$/\n/')
fi

# copy the target itself into the directory also
cp -p $1 $2
if [ "$6" = 1 ]; then
    # for the copied target on mac change all the non-absolute paths to just the basename
    echo "$deps" | while read x;
    do
        if [[ $x =~ ^@ ]]; then
            install_name_tool -change $x `basename $x` $2/`basename $1`;
        fi
    done
fi

echo; echo "All dependencies"
echo "$deps"

# filter dependencies with expression
#echo "$3" "$4"
#deps=$(echo "$deps" | grep -Ei $3 | grep -Eiv $4)
if [[ !  -z  $3  ]]; then
    deps=$(echo "$deps" | grep -E${IGNORE_CASE} "$3")
fi
if [[ !  -z  $4  ]]; then
    deps=$(echo "$deps" | grep -E${IGNORE_CASE}v "$4")
fi
echo; echo "Filtered dependencies"
echo "$deps"

echo; echo "Copying the dependencies to $2"
echo "$deps" | while read x;
do
    [ -z "$x" ] && continue
    bn=`basename $x`
    if [ -f $x ]; then
        # library has absolute path, copy as is
        cp $x $2 && chmod a+wx $2/$bn
    else
        # go through the search path to see if the library is in any of those locations
        echo "$search" | while read y;
        do
            if [ -f $y/$bn ]; then
                # found library in search path, copy from there 
                cp $y/$bn $2 && chmod a+wx $2/$bn
            fi
        done # search path
    fi # libary not found
done # for each dep

if [ "$6" = 1 ]; then
    echo; echo "Modifying loader paths"
    for f in $2/*
    do
        if [ -f $f ]; then
            echo "$deps" | while read x;
            do
                bn=`basename $x`
                install_name_tool -change $x @executable_path/$bn $f;
                # incase the rpath was stripped off
                install_name_tool -change $bn @executable_path/$bn $f;
            done # for each dep
        fi # if file
    done # for each copied file
fi # use otool

echo "Done!"

