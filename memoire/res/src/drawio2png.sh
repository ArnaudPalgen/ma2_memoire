find . -name '*.drawio' -exec rm -f {}.png \; -exec drawio -x -o ../{}.png {} \;