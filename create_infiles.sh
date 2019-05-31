#!/bin/bash

function generateFile {
    charsNum=$(shuf -i 1000-128000 -n 1) # random integer between 1000 and 128000
    curFileContents=$(cat /dev/urandom | tr -dc A-Za-z0-9 | head -c $charsNum) # random alpharithmetic string that is $charsNum characters long

    echo "$curFileContents" > "$curFilePath"    # write to file
}

# check number of arguments
if [[ "$#" -ne 4 ]]; then
    echo "Illegal number of parameters";  exit 1
fi

dirName=$1      # name of directory in which the files and directories will be created
filesNum=$2     # number of files that will be created
dirsNum=$3      # number of directories that will be created
levelsNum=$4    # number of levels allowed

args=("$@") # array of all arguments

integerRegex='^[0-9]+$' # positive integer regular expression

for arg in "${args[@]}"
do
    if [[ $arg == $1 ]] || [[ $arg == $0 ]]; then   # we don't have to check the validity of these arguments
        continue
    fi

    # check if current argument is a positive integer
    if ! [[ $arg =~ $integerRegex ]]; then
        echo "Argument $arg is not a positive integer"; exit 1
    fi
done

# create directory if it does not exist
mkdir -p $dirName

dirNames=()         # array of directories' generated names
dirPaths=()         # array of directories' paths
fileNames=()        # array of files' generated names

for ((i=0; i<$filesNum; i++));
do
    charsNum=$(shuf -i 1-8 -n 1) # random integer between 1 and 8
    fileNames[i]=$(cat /dev/urandom | tr -dc A-Za-z0-9 | head -c $charsNum)  # random alpharithmetic string that is $charsNum characters long
done

levelsNumTemp=0     # temporary level number in each for loop
dirPaths[0]=$dirName # add parent directory's name as the first element of the dirPath's array
for ((i=1; i<$dirsNum + 1; i++));
do
    # create directories
    charsNum=$(shuf -i 1-8 -n 1)  # random integer between 1 and 8
    dirNames[i]=$(cat /dev/urandom | tr -dc A-Za-z0-9 | head -c $charsNum) # random alpharithmetic string that is $charsNum characters long

    curDirPath=${dirNames[i]}   # current directory's generated name

    # concatenate current directory's name with the previous levels' directories' names to form the current directory's path
    for ((j=1; j<=$levelsNumTemp; j++));
    do
        curDirPath="${dirNames[i - j]}/$curDirPath"
    done

    curDirPath="$dirName/$curDirPath"    # include the parent dir's name
    echo "Created directory $curDirPath" # print current directory's path

    mkdir -p $curDirPath                 # create current directory

    dirPaths[i]=$curDirPath     # store current directory's path

    ((levelsNumTemp ++))        # proceed current level

    if (( $levelsNumTemp == $levelsNum )); then     # level's number exceeds the maximum number that was given by the user
        levelsNumTemp=0                             # reset current level to 0 level
    fi
done

fileNamesIndex=0    # next index in fileNames array in each for loop
while (( $fileNamesIndex < $filesNum ));
do
    if (( $dirsNum == 0 )); then
        # create all files in the same level in the parent directory
        curFilePath="$dirName/${fileNames[$fileNamesIndex]}" # build current file's path
        touch "$curFilePath"                                 # create file in parent directory in the 1st level
        echo "Created file $curFilePath"                     # print created file's path

        generateFile

        ((fileNamesIndex ++))       # proceed index of fileNames' array

        continue
    fi

    for dirPath in "${dirPaths[@]}"
    do
        # create files in a round-robin manner
        curFilePath="$dirPath/${fileNames[$fileNamesIndex]}" # build current file's path
        touch "$curFilePath"                                 # create file in current directory
        echo "Created file $curFilePath"                     # print created file's path

        generateFile

        ((fileNamesIndex ++))       # proceed index of fileNames' array

        if (( $fileNamesIndex >= $filesNum )); then     # if we exceeded the files number that was provided by the user
            break
        fi
    done
done