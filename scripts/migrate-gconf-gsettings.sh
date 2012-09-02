#!/bin/bash

a=""
IFS=
XMLFEEDS=`gconftool-2 -g /apps/evolution/evolution-rss/feeds`
if [ $XMLFEEDS == 'false' ]; then
	echo "Feeds already converted. Nothing to do!"
	exit 0
fi
for i in $XMLFEEDS; do
if [ -z $a ]; then
	a=$a$i
else
	a=$a"@"$i
fi
done
IFS=$'\n'
b=`echo $a|sed -e "s!\@!,!g"`
echo "gconf->dconf feeds migration done."
gconftool-2 -s /apps/evolution/evolution-rss/feeds --type bool 0
dconf write /org/gnome/evolution/plugin/rss/feeds \"$a\"
