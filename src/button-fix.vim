
fun Go()
	g/\s\+BUTTON/- put='/* =============================================================='
	:wq
endfun

au VimEnter * :silent! call Go()
