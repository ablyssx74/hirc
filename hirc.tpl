name			$(NAME)
version			$(VERSION)-1
architecture	$(ARCH)
summary 		"hirc"
description 	"hirc - A lightweight, multi-server IRC client."
packager		"ablyss <hirc@epluribusunix.net>"
vendor			"epluribusunix.net Project"
licenses {
	"MIT"
}
copyrights {
	"$(YEAR) ablyss"
}
provides {
	$(NAME) = $(VERSION)-1
}
requires {
	haiku
	blink
}	
urls {
	"https://github.com/ablyssx74/hirc"
}
