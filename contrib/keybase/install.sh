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
echo "You'll need to include a path to '~/.neomutt/keybaseMutt/scripts' in your shell's rc file. If you've done this previously on your computer, press 'n'."
echo "Do you use [b]ash, [k]sh, or [z]sh? [n]"
echo "(You use $SHELL)"
read -r shellInput
if [ "$shellInput" = 'b' ]; then
	echo 'export PATH="$PATH:~/.neomutt/keybaseMutt/scripts"' >> "$HOME/.bashrc"
elif [ "$shellInput" = 'k' ]; then
	echo 'export PATH="$PATH:~/.neomutt/keybaseMutt/scripts"' >> "$HOME/.kshrc"
elif [ "$shellInput" = 'z' ]; then
	echo 'export PATH="$PATH:~/.neomutt/keybaseMutt/scripts"' >> "$HOME/.zshrc"
else
	echo "If you use something another shell, you'll need to add the path manually."
fi

echo "Please restart your shell to be able to use the scripts (closing and reopening the terminal is easiest)."
