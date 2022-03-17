#!/bin/sh

# If no directory exists, make it exist.
if [ -d "$HOME/.neomutt/keybaseMutt" ]; then

	# If someone already has a backup, complain.
	if [ -d "$HOME/.neomutt/keybaseMuttBACKUP" ]; then
		#echo "$HOME/.neomutt/keybaseMuttBACKUP exists"
		echo "You are going to overwrite your backup. Are you sure you want to do this? [y|n]"
		read -r overwrite

		# If they want to delete their backup.
		if [ "$overwrite" = 'y' ]; then
			cp -R "$HOME/.neomutt/keybaseMutt" "$HOME/.neomutt/keybaseMuttBACKUP"
			rm -r "$HOME/.neomutt/keybaseMutt"
			mkdir -p "$HOME/.neomutt/keybaseMutt/scripts"

		# Otherwise, abort mission.
		else
			echo "ABORT! ABORT! ABORT!"
			exit 1
		fi

	elif [ ! -d "$HOME/.neomutt/keybaseMuttBACKUP" ]; then
		echo "Backing up previous install."
		cp -R "$HOME/.neomutt/keybaseMutt" "$HOME/.neomutt/keybaseMuttBACKUP"
		rm -r "$HOME/.neomutt/keybaseMutt"
		mkdir -p "$HOME/.neomutt/keybaseMutt/scripts"
	fi
# Otherwise, make a backup
elif [ ! -d "$HOME/.neomutt/keybaseMutt" ]; then
	echo "Installing your program..."
	mkdir -p "$HOME/.neomutt/keybaseMutt/scripts"

fi

# Copy my directory to your directory.
cp ./keybase.py  "$HOME/.neomutt/keybaseMutt"
cp ./pgpdecrypt.sh "$HOME/.neomutt/keybaseMutt/scripts"
cp ./decrypt.sh "$HOME/.neomutt/keybaseMutt/scripts"
cp ./verify.sh "$HOME/.neomutt/keybaseMutt/scripts"
cp ./pgpverify.sh "$HOME/.neomutt/keybaseMutt/scripts"

# Yay! Stuff's installed!
echo "You'll need to include '~/.neomutt/keybaseMutt/scripts' in your \$PATH."
echo "This can be done manually by installing in your ~/.profile."
echo "If you've done this previously on your computer, select 'n'."
echo "Do you want to proceed? [n]"
read -r shellInput
[ -n "$shellInput" ] && [ "$shellInput" != 'n' ] && echo 'PATH="$PATH:$HOME/.neomutt/keybaseMutt/scripts"' >> "$HOME/.profile"

echo "Please restart your shell to be able to use the scripts ('exec $SHELL' or reopening the terminal is easiest)."
