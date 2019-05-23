#!/bin/bash

function lookup_lang()
{
	case "${1##*/}" in
		eu)    IMG="basque.png";              NAME="Basque";;
		bg)    IMG="bulgarian.png";           NAME="Bulgarian";;
		ca)    IMG="catalan.png";             NAME="Catalan";;
		zh_CN) IMG="chinese-simplified.png";  NAME="Chinese (Simplified)";;
		zh_TW) IMG="chinese-traditional.png"; NAME="Chinese (Traditional)";;
		cs)    IMG="czech.png";               NAME="Czech";;
		da)    IMG="danish.png";              NAME="Danish";;
		nl)    IMG="dutch.png";               NAME="Dutch";;
		en_GB) IMG="english.png";             NAME="English (British)";;
		eo)    IMG="esperanto.png";           NAME="Esperanto";;
		et)    IMG="estonian.png";            NAME="Estonian";;
		fi)    IMG="finnish.png";             NAME="Finnish";;
		fr)    IMG="french.png";              NAME="French";;
		gl)    IMG="galician.png";            NAME="Galician";;
		de)    IMG="german.png";              NAME="German";;
		el)    IMG="greek.png";               NAME="Greek";;
		hu)    IMG="hungarian.png";           NAME="Hungarian";;
		id)    IMG="indonesian.png";          NAME="Indonesian";;
		ga)    IMG="irish.png";               NAME="Irish";;
		it)    IMG="italian.png";             NAME="Italian";;
		ja)    IMG="japanese.png";            NAME="Japanese";;
		ko)    IMG="korean.png";              NAME="Korean";;
		lt)    IMG="lithuanian.png";          NAME="Lithuanian";;
		pl)    IMG="polish.png";              NAME="Polish";;
		pt_BR) IMG="portuguese-brazil.png";   NAME="Portuguese (Brazil)";;
		ru)    IMG="russian.png";             NAME="Russian";;
		sk)    IMG="slovak.png";              NAME="Slovak";;
		es)    IMG="spanish.png";             NAME="Spanish";;
		sv)    IMG="swedish.png";             NAME="Swedish";;
		tr)    IMG="turkish.png";             NAME="Turkish";;
		uk)    IMG="ukrainian.png";           NAME="Ukrainian";;
	esac
}

function html_header()
{
	echo "---"
	echo "layout: concertina"
	echo "title: Translations"
	echo "---"
	echo ""
	echo "<h2>Translating NeoMutt</h2>"
	echo ""
	echo "<p>"
	echo "  NeoMutt has been translated into 30 languages."
	echo "  Unfortunately, some of the translations are out-of-date."
	echo "</p>"
	echo ""
	echo "<p>"
	echo "  Do <b>YOU</b> speak one of these languages?"
	echo "  If so, <a href=\"mailto:rich@flatcap.org\">please help us</a>."
	echo "</p>"
	echo ""
	echo "<p>"
	echo "  Read more about what would be involved in"
	echo "  <a href=\"/dev/translate\">translating NeoMutt</a>."
	echo "</p>"
	echo ""
	echo "<table class=\"lang\" summary=\"list of languages\">"
	echo "  <thead>"
	echo "    <tr>"
	echo "      <th>Language</th>"
	echo "      <th style=\"text-align: center;\">"
	echo "      <span style=\"border: 1px solid black; background: #6f6; padding: 3px;\">Complete</span>"
	echo "      <span style=\"border: 1px solid black; background: #ff6; padding: 3px;\">Fuzzy</span>"
	echo "      <span style=\"border: 1px solid black; background: #f66; padding: 3px;\">Incomplete</span>"
	echo "      </th>"
	echo "    </tr>"
	echo "  </thead>"
	echo "  <tbody>"
	echo "    <tr>"
	echo "      <td><img src=\"/images/flags/english.png\">English</td>"
	echo "      <td>"
	echo "      <div style=\"background: #6f6; min-width: 500px;\">Base Language</div></td>"
	echo "    </tr>"
}

function html_line()
{
	local LINE="$1"
	local LANG=""
	local TNUM=0
	local FNUM=0
	local UNUM=0

	# filename: 104 translated, 22 fuzzy, 11 untranslated
	if [[ "$LINE" =~ (.*):[[:space:]]+([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]fuzzy,[[:space:]]([0-9]+)[[:space:]]untranslated ]]; then
		LANG="${BASH_REMATCH[1]}"
		TNUM=${BASH_REMATCH[2]} # translated
		FNUM=${BASH_REMATCH[3]} # fuzzy
		UNUM=${BASH_REMATCH[4]} # untranslated
	# filename: 320 translated, 20 untranslated
	elif [[ "$LINE" =~ (.*):[[:space:]]+([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]untranslated ]]; then
		LANG="${BASH_REMATCH[1]}"
		TNUM=${BASH_REMATCH[2]} # translated
		UNUM=${BASH_REMATCH[3]} # untranslated
	# filename: 5 translated, 13 fuzzy
	elif [[ "$LINE" =~ (.*):[[:space:]]+([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]fuzzy ]]; then
		LANG="${BASH_REMATCH[1]}"
		TNUM=${BASH_REMATCH[2]} # translated
		FNUM=${BASH_REMATCH[3]} # fuzzy
	# filename: 63 translated
	elif [[ "$LINE" =~ (.*):[[:space:]]+([0-9]+)[[:space:]]translated ]]; then
		LANG="${BASH_REMATCH[1]}"
		TNUM=${BASH_REMATCH[2]} # translated
	else
		return
	fi

	lookup_lang "$LANG"

	local TOTAL=$((TNUM+FNUM+UNUM)) # number of translated strings
	local PC=$((100*TNUM/TOTAL)) # percentage complete

	local TPX=$((500*TNUM/TOTAL)) # pixels for translated
	local FPX=$((500*FNUM/TOTAL)) # pixels for fuzzy
	local UPX=$((500*UNUM/TOTAL)) # pixels for untranslated

	[ $FPX -gt 0 ] && [ $FPX -lt 10 ] && TPX=$((TPX-5)) # adjust for the size of a non-breaking space
	[ $UPX -gt 0 ] && [ $UPX -lt 10 ] && TPX=$((TPX-7))

	local TITLE="$NAME: $TNUM translated"
	[ $FNUM -gt 0 ] && TITLE="$TITLE, $FNUM fuzzy"
	[ $UNUM -gt 0 ] && TITLE="$TITLE, $UNUM untranslated"

	echo "    <tr title=\"$TITLE\">"
	echo "      <td><img src=\"/images/flags/$IMG\">$NAME</td>"
	echo "      <td>"
	if [ $TPX -gt 0 ]; then
		echo "        <div style=\"background: #6f6; min-width: ${TPX}px;\">${PC}%</div>"
	fi
	if [ $FPX -gt 0 ]; then
		echo "        <div style=\"background: #ff6; min-width: ${FPX}px; border-left: 1px solid black; border-right: 1px solid black;\">&#160;</div>"
	fi
	if [ $UPX -gt 0 ]; then
		echo "        <div style=\"background: #f66; min-width: ${UPX}px;\">&#160;</div>"
	fi
	echo "      </td>"
	echo "    </tr>"
}

function html_footer()
{
	echo "  </tbody>"
	echo "</table>"
	echo "<br>"
	echo "Last updated: $(date --utc '+%F %R') UTC"
}


[ $# = 0 ] && ARGS="*.po" || ARGS="$*"

html_header
for i in $ARGS; do
	echo -ne "${i%.po}:\t"
	msgfmt --statistics -c -o /dev/null "$i" 2>&1
done \
	| grep -wv "en_GB" \
	| sed 's/ \(message\|translation\)s*\.*//g' \
	| sort -nr -k2 -k4 -k6 \
	| while read -r line; do
	html_line "$line"
done
html_footer

