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
cp keybase.py "$HOME/.neomutt/keybaseMutt"
cp pgpdecrypt.sh decrypt.sh verify.sh pgpverify.sh "$HOME/.neomutt/keybaseMutt/scripts"

# Yay! Stuff's installed!
if ! awk 'BEGIN {split(ENVIRON["PATH"], path, ":"); for(i = 1; i <= length(path); ++i) if(path[i] == (ENVIRON["HOME"] "/.neomutt/keybaseMutt/scripts")) exit 0; exit 1}'; then
	echo "You'll need to include '~/.neomutt/keybaseMutt/scripts' in your \$PATH."
	echo "This can be done manually by installing in your ~/.profile."
	echo "If you've done this previously on your computer, select 'n'."
	echo "Do you want to proceed? [n]"
	read -r shellInput
	[ -n "$shellInput" ] && [ "$shellInput" != 'n' ] && echo 'PATH="$PATH:$HOME/.neomutt/keybaseMutt/scripts"' >> "$HOME/.profile"
fi

echo "Please restart your shell to be able to use the scripts ('exec $SHELL' or reopening the terminal is easiest)."
