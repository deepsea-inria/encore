all: pldi18.html

%.pdf : %.md
	pandoc $< -s -o $@

%.html : %.md
	pandoc $< -s -c pldi18.css -o $@

clean:
	rm -f *.html *.pdf
