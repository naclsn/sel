let SessionLoad = 1
let s:cpo_save=&cpo
set cpo&vim
cnoremap <C-X> 
cnoremap <C-F> <Right>
cnoremap <C-D> <Del>
cnoremap <C-B> <Left>
cnoremap <C-A> <Home>
map! <C-Space> <Nop>
inoremap <C-W> u
inoremap <C-U> u
nnoremap  <Cmd>nohlsearch|diffupdate|normal! 
nmap  d
omap <silent> % <Plug>(MatchitOperationForward)
xmap <silent> % <Plug>(MatchitVisualForward)
nmap <silent> % <Plug>(MatchitNormalForward)
nnoremap & :&&
noremap <expr> , get(g:,'kae',',')
noremap <expr> ; get(g:,'eak',';')
xnoremap <silent> <expr> @ mode() ==# 'V' ? ':normal! @'.getcharstr().'' : '@'
nnoremap F :unl! g:eak g:kaeF
xnoremap <silent> <expr> Q mode() ==# 'V' ? ':normal! @=reg_recorded()' : 'Q'
nnoremap T :unl! g:eak g:kaeT
nnoremap Y y$
nnoremap ZD` mzvi`lxgvohx`z
nnoremap ZR`` mzvi`lr`gvohr``z
nnoremap ZR`' mzvi`lr'gvohr'`z
nnoremap ZR`" mzvi`lr"gvohr"`z
nnoremap ZR`> mzvi`lr>gvohr<`z
nnoremap ZR`< mzvi`lr>gvohr<`z
nnoremap ZR`] mzvi`lr]gvohr[`z
nnoremap ZR`[ mzvi`lr]gvohr[`z
nnoremap ZR`} mzvi`lr}gvohr{`z
nnoremap ZR`{ mzvi`lr}gvohr{`z
nnoremap ZR`) mzvi`lr)gvohr(`z
nnoremap ZR`( mzvi`lr)gvohr(`z
nnoremap ZD' mzvi'lxgvohx`z
nnoremap ZR'` mzvi'lr`gvohr``z
nnoremap ZR'' mzvi'lr'gvohr'`z
nnoremap ZR'" mzvi'lr"gvohr"`z
nnoremap ZR'> mzvi'lr>gvohr<`z
nnoremap ZR'< mzvi'lr>gvohr<`z
nnoremap ZR'] mzvi'lr]gvohr[`z
nnoremap ZR'[ mzvi'lr]gvohr[`z
nnoremap ZR'} mzvi'lr}gvohr{`z
nnoremap ZR'{ mzvi'lr}gvohr{`z
nnoremap ZR') mzvi'lr)gvohr(`z
nnoremap ZR'( mzvi'lr)gvohr(`z
nnoremap ZD" mzvi"lxgvohx`z
nnoremap ZR"` mzvi"lr`gvohr``z
nnoremap ZR"' mzvi"lr'gvohr'`z
nnoremap ZR"" mzvi"lr"gvohr"`z
nnoremap ZR"> mzvi"lr>gvohr<`z
nnoremap ZR"< mzvi"lr>gvohr<`z
nnoremap ZR"] mzvi"lr]gvohr[`z
nnoremap ZR"[ mzvi"lr]gvohr[`z
nnoremap ZR"} mzvi"lr}gvohr{`z
nnoremap ZR"{ mzvi"lr}gvohr{`z
nnoremap ZR") mzvi"lr)gvohr(`z
nnoremap ZR"( mzvi"lr)gvohr(`z
nnoremap ZD> mzvi<lxgvohx`z
nnoremap ZR>` mzvi<lr`gvohr``z
nnoremap ZR>' mzvi<lr'gvohr'`z
nnoremap ZR>" mzvi<lr"gvohr"`z
nnoremap ZR>> mzvi<lr>gvohr<`z
nnoremap ZR>< mzvi<lr>gvohr<`z
nnoremap ZR>] mzvi<lr]gvohr[`z
nnoremap ZR>[ mzvi<lr]gvohr[`z
nnoremap ZR>} mzvi<lr}gvohr{`z
nnoremap ZR>{ mzvi<lr}gvohr{`z
nnoremap ZR>) mzvi<lr)gvohr(`z
nnoremap ZR>( mzvi<lr)gvohr(`z
nnoremap ZD< mzvi<lxgvohx`z
nnoremap ZR<` mzvi<lr`gvohr``z
nnoremap ZR<' mzvi<lr'gvohr'`z
nnoremap ZR<" mzvi<lr"gvohr"`z
nnoremap ZR<> mzvi<lr>gvohr<`z
nnoremap ZR<< mzvi<lr>gvohr<`z
nnoremap ZR<] mzvi<lr]gvohr[`z
nnoremap ZR<[ mzvi<lr]gvohr[`z
nnoremap ZR<} mzvi<lr}gvohr{`z
nnoremap ZR<{ mzvi<lr}gvohr{`z
nnoremap ZR<) mzvi<lr)gvohr(`z
nnoremap ZR<( mzvi<lr)gvohr(`z
nnoremap ZD] mzvi[lxgvohx`z
nnoremap ZR]` mzvi[lr`gvohr``z
nnoremap ZR]' mzvi[lr'gvohr'`z
nnoremap ZR]" mzvi[lr"gvohr"`z
nnoremap ZR]> mzvi[lr>gvohr<`z
nnoremap ZR]< mzvi[lr>gvohr<`z
nnoremap ZR]] mzvi[lr]gvohr[`z
nnoremap ZR][ mzvi[lr]gvohr[`z
nnoremap ZR]} mzvi[lr}gvohr{`z
nnoremap ZR]{ mzvi[lr}gvohr{`z
nnoremap ZR]) mzvi[lr)gvohr(`z
nnoremap ZR]( mzvi[lr)gvohr(`z
nnoremap ZD[ mzvi[lxgvohx`z
nnoremap ZR[` mzvi[lr`gvohr``z
nnoremap ZR[' mzvi[lr'gvohr'`z
nnoremap ZR[" mzvi[lr"gvohr"`z
nnoremap ZR[> mzvi[lr>gvohr<`z
nnoremap ZR[< mzvi[lr>gvohr<`z
nnoremap ZR[] mzvi[lr]gvohr[`z
nnoremap ZR[[ mzvi[lr]gvohr[`z
nnoremap ZR[} mzvi[lr}gvohr{`z
nnoremap ZR[{ mzvi[lr}gvohr{`z
nnoremap ZR[) mzvi[lr)gvohr(`z
nnoremap ZR[( mzvi[lr)gvohr(`z
nnoremap ZD} mzvi{lxgvohx`z
nnoremap ZR}` mzvi{lr`gvohr``z
nnoremap ZR}' mzvi{lr'gvohr'`z
nnoremap ZR}" mzvi{lr"gvohr"`z
nnoremap ZR}> mzvi{lr>gvohr<`z
nnoremap ZR}< mzvi{lr>gvohr<`z
nnoremap ZR}] mzvi{lr]gvohr[`z
nnoremap ZR}[ mzvi{lr]gvohr[`z
nnoremap ZR}} mzvi{lr}gvohr{`z
nnoremap ZR}{ mzvi{lr}gvohr{`z
nnoremap ZR}) mzvi{lr)gvohr(`z
nnoremap ZR}( mzvi{lr)gvohr(`z
nnoremap ZD{ mzvi{lxgvohx`z
nnoremap ZR{` mzvi{lr`gvohr``z
nnoremap ZR{' mzvi{lr'gvohr'`z
nnoremap ZR{" mzvi{lr"gvohr"`z
nnoremap ZR{> mzvi{lr>gvohr<`z
nnoremap ZR{< mzvi{lr>gvohr<`z
nnoremap ZR{] mzvi{lr]gvohr[`z
nnoremap ZR{[ mzvi{lr]gvohr[`z
nnoremap ZR{} mzvi{lr}gvohr{`z
nnoremap ZR{{ mzvi{lr}gvohr{`z
nnoremap ZR{) mzvi{lr)gvohr(`z
nnoremap ZR{( mzvi{lr)gvohr(`z
nnoremap ZD) mzvi(lxgvohx`z
nnoremap ZR)` mzvi(lr`gvohr``z
nnoremap ZR)' mzvi(lr'gvohr'`z
nnoremap ZR)" mzvi(lr"gvohr"`z
nnoremap ZR)> mzvi(lr>gvohr<`z
nnoremap ZR)< mzvi(lr>gvohr<`z
nnoremap ZR)] mzvi(lr]gvohr[`z
nnoremap ZR)[ mzvi(lr]gvohr[`z
nnoremap ZR)} mzvi(lr}gvohr{`z
nnoremap ZR){ mzvi(lr}gvohr{`z
nnoremap ZR)) mzvi(lr)gvohr(`z
nnoremap ZR)( mzvi(lr)gvohr(`z
nnoremap ZD( mzvi(lxgvohx`z
nnoremap ZR(` mzvi(lr`gvohr``z
nnoremap ZR(' mzvi(lr'gvohr'`z
nnoremap ZR(" mzvi(lr"gvohr"`z
nnoremap ZR(> mzvi(lr>gvohr<`z
nnoremap ZR(< mzvi(lr>gvohr<`z
nnoremap ZR(] mzvi(lr]gvohr[`z
nnoremap ZR([ mzvi(lr]gvohr[`z
nnoremap ZR(} mzvi(lr}gvohr{`z
nnoremap ZR({ mzvi(lr}gvohr{`z
nnoremap ZR() mzvi(lr)gvohr(`z
nnoremap ZR(( mzvi(lr)gvohr(`z
omap <silent> [% <Plug>(MatchitOperationMultiBackward)
xmap <silent> [% <Plug>(MatchitVisualMultiBackward)
nmap <silent> [% <Plug>(MatchitNormalMultiBackward)
omap <silent> ]% <Plug>(MatchitOperationMultiForward)
xmap <silent> ]% <Plug>(MatchitVisualMultiForward)
nmap <silent> ]% <Plug>(MatchitNormalMultiForward)
xmap a% <Plug>(MatchitVisualTextObject)
nnoremap f :unl! g:eak g:kaef
omap <silent> g% <Plug>(MatchitOperationBackward)
xmap <silent> g% <Plug>(MatchitVisualBackward)
nmap <silent> g% <Plug>(MatchitNormalBackward)
nnoremap t :unl! g:eak g:kaet
xmap <silent> <Plug>(MatchitVisualTextObject) <Plug>(MatchitVisualMultiBackward)o<Plug>(MatchitVisualMultiForward)
onoremap <silent> <Plug>(MatchitOperationMultiForward) :call matchit#MultiMatch("W",  "o")
onoremap <silent> <Plug>(MatchitOperationMultiBackward) :call matchit#MultiMatch("bW", "o")
xnoremap <silent> <Plug>(MatchitVisualMultiForward) :call matchit#MultiMatch("W",  "n")m'gv``
xnoremap <silent> <Plug>(MatchitVisualMultiBackward) :call matchit#MultiMatch("bW", "n")m'gv``
nnoremap <silent> <Plug>(MatchitNormalMultiForward) :call matchit#MultiMatch("W",  "n")
nnoremap <silent> <Plug>(MatchitNormalMultiBackward) :call matchit#MultiMatch("bW", "n")
onoremap <silent> <Plug>(MatchitOperationBackward) :call matchit#Match_wrapper('',0,'o')
onoremap <silent> <Plug>(MatchitOperationForward) :call matchit#Match_wrapper('',1,'o')
xnoremap <silent> <Plug>(MatchitVisualBackward) :call matchit#Match_wrapper('',0,'v')m'gv``
xnoremap <silent> <Plug>(MatchitVisualForward) :call matchit#Match_wrapper('',1,'v'):if col("''") != col("$") | exe ":normal! m'" | endifgv``
nnoremap <silent> <Plug>(MatchitNormalBackward) :call matchit#Match_wrapper('',0,'n')
nnoremap <silent> <Plug>(MatchitNormalForward) :call matchit#Match_wrapper('',1,'n')
nmap <C-W><C-D> d
nnoremap <C-L> <Cmd>nohlsearch|diffupdate|normal! 
cnoremap  <Home>
cnoremap  <Left>
cnoremap  <Del>
cnoremap  <Right>
inoremap  u
inoremap  u
cnoremap  
lmap :S ]
lmap :A ?
lmap :L :
lmap :D "
lmap :K @
lmap : P
lmap :J M
lmap ;s )
lmap ;a /
lmap ;l ;
lmap ;f <BS>
lmap ;d '
lmap ;k !
lmap ; p
lmap ;j m
lmap AL [
lmap A: ?
lmap AK Z
lmap AD X
lmap AS W
lmap AJ Q
lmap AF F
lmap A A
lmap DL _
lmap DK <
lmap D: "
lmap DJ Y
lmap DA X
lmap DF R
lmap D E
lmap DS D
lmap FK V
lmap F T
lmap FD R
lmap FL G
lmap FA F
lmap FS C
lmap FJ B
lmap JD Y
lmap JL U
lmap JA Q
lmap J N
lmap J: M
lmap JS J
lmap JK H
lmap JF B
lmap KD <
lmap K: @
lmap KA Z
lmap KF V
lmap KL L
lmap KS K
lmap K I
lmap KJ H
lmap LD _
lmap LA [
lmap LS >
lmap L: :
lmap LJ U
lmap L O
lmap LK L
lmap LF G
lmap S: ]
lmap SL >
lmap SA W
lmap S S
lmap SK K
lmap SJ J
lmap SD D
lmap SF C
lmap al (
lmap a; /
lmap ak z
lmap ad x
lmap as w
lmap aj q
lmap af f
lmap a a
lmap dl -
lmap dk ,
lmap d; '
lmap dj y
lmap da x
lmap df r
lmap d e
lmap ds d
lmap f; <BS>
lmap fk v
lmap f t
lmap fd r
lmap fl g
lmap fa f
lmap fs c
lmap fj b
lmap jd y
lmap jl u
lmap ja q
lmap j n
lmap j; m
lmap js j
lmap jk h
lmap jf b
lmap kd ,
lmap k; !
lmap ka z
lmap kf v
lmap kl l
lmap ks k
lmap k i
lmap kj h
lmap ld -
lmap la (
lmap ls .
lmap l; ;
lmap lj u
lmap l o
lmap lk l
lmap lf g
lmap s; )
lmap sl .
lmap sa w
lmap s s
lmap sk k
lmap sj j
lmap sd d
lmap sf c
cabbr vb vert sb
cabbr pw setl pvw!
cabbr wr setl wrap! bri!
cabbr hl setl hls!
cabbr scra setl bt=nofile ft
cabbr lang setl wrap! bri! spell! spl
let &cpo=s:cpo_save
unlet s:cpo_save
set backspace=
set completeopt=menuone,noselect
set directory=~/.vim/cache/swap//
set noequalalways
set expandtab
set formatoptions=1crqnlj
set helplang=en
set nohlsearch
set isfname=@,48-57,/,.,-,_,+,,,#,$,%,~
set listchars=tab:>\ ,trail:~
set mouse=nrv
set runtimepath=~/.vim/,~/.config/nvim,/etc/xdg/xdg-ubuntu/nvim,/etc/xdg/nvim,~/.local/share/nvim/site,~/.local/share/nvim/site/pack/*/start/*,/usr/share/ubuntu/nvim/site,/usr/share/gnome/nvim/site,/usr/local/share/nvim/site,/usr/share/nvim/site,/var/lib/snapd/desktop/nvim/site,/opt/neovim/share/nvim/runtime,/opt/neovim/share/nvim/runtime/pack/dist/opt/matchit,/opt/neovim/lib/nvim,/var/lib/snapd/desktop/nvim/site/after,/usr/share/nvim/site/after,/usr/local/share/nvim/site/after,/usr/share/gnome/nvim/site/after,/usr/share/ubuntu/nvim/site/after,~/.local/share/nvim/site/after,/etc/xdg/nvim/after,/etc/xdg/xdg-ubuntu/nvim/after,~/.config/nvim/after
set sessionoptions=blank,buffers,folds,globals,options,resize,sesdir,slash,tabpages,terminal,unix,winsize
set shiftwidth=0
set spellcapcheck=
set spellfile=~/.vim/spell/my.utf-8.add
set tabstop=4
set notimeout
set timeoutlen=100
set ttimeoutlen=100
set undodir=~/.vim/cache/nundo//
set undofile
set wildmode=longest:full,full
set wildoptions=pum
set window=37
set winminheight=0
set winminwidth=0
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
exe "cd " . escape(expand("<sfile>:p:h"), ' ')
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +21 etc/sel.vim
badd +124 src/prelude.sel
badd +1 src/builtin.rs
badd +187 README.md
badd +13 NOTE.md
badd +1 src/interp.rs
badd +61 src/main.rs
badd +12 Cargo.toml
badd +143 src/format.rs
badd +78 src/parse.rs
argglobal
%argdel
$argadd ~/Documents/Projects/sel/etc/sel.vim
set lines=38 columns=158
set stal=2
tabnew +setlocal\ bufhidden=wipe
tabnew +setlocal\ bufhidden=wipe
tabrewind
edit src/interp.rs
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 105 + 79) / 158)
exe 'vert 2resize ' . ((&columns * 52 + 79) / 158)
argglobal
balt src/builtin.rs
onoremap <buffer> <silent> [[ :call rust#Jump('o', 'Back')
xnoremap <buffer> <silent> [[ :call rust#Jump('v', 'Back')
nnoremap <buffer> <silent> [[ :call rust#Jump('n', 'Back')
onoremap <buffer> <silent> ]] :call rust#Jump('o', 'Forward')
xnoremap <buffer> <silent> ]] :call rust#Jump('v', 'Forward')
nnoremap <buffer> <silent> ]] :call rust#Jump('n', 'Forward')
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal cinoptions=L0,(s,Ws,J1,j1,m1
setlocal cinscopedecls=public,protected,private
setlocal cinwords=for,if,else,while,loop,impl,mod,unsafe,trait,struct,enum,fn,extern,macro
setlocal colorcolumn=
setlocal comments=s0:/*!,ex:*/,s1:/*,mb:*,ex:*/,:///,://!,://
setlocal commentstring=//\ %s
setlocal complete=.,w,b,u,t
setlocal completefunc=
setlocal completeslash=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal cursorlineopt=both
setlocal nodiff
setlocal errorformat=%-G,%-Gerror:\ aborting\ %.%#,%-Gerror:\ Could\ not\ compile\ %.%#,%Eerror:\ %m,%Eerror[E%n]:\ %m,%Wwarning:\ %m,%Inote:\ %m,%C\ %#-->\ %f:%l:%c,%E\ \ left:%m,%C\ right:%m\ %f:%l:%c,%Z,%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:%c:\ %*\\d:%*\\d\ %t%*[^:]:\ %m,%-G%f:%l\ %s,%-G%*[\ ]^,%-G%*[\ ]^%*[~],%-G%*[\ ]...,%-G%\\s%#Downloading%.%#,%-G%\\s%#Checking%.%#,%-G%\\s%#Compiling%.%#,%-G%\\s%#Finished%.%#,%-G%\\s%#error:\ Could\ not\ compile\ %.%#,%-G%\\s%#To\ learn\ more\\,%.%#,%-G%\\s%#For\ more\ information\ about\ this\ error\\,%.%#,%-Gnote:\ Run\ with\ `RUST_BACKTRACE=%.%#,%.%#panicked\ at\ \\'%m\\'\\,\ %f:%l:%c
setlocal expandtab
if &filetype != 'rust'
setlocal filetype=rust
endif
setlocal fixendofline
setlocal foldcolumn=0
set nofoldenable
setlocal nofoldenable
setlocal foldexpr=0
setlocal foldignore=#
set foldlevel=999
setlocal foldlevel=999
setlocal foldmarker={{{,}}}
set foldmethod=marker
setlocal foldmethod=marker
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=1crqnlj
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=\\v^\\s*(pub\\s+)?use\\s+\\zs(\\f|:)+
setlocal includeexpr=rust#IncludeExpr(v:fname)
setlocal indentexpr=GetRustIndent(v:lnum)
setlocal indentkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
set linebreak
setlocal linebreak
setlocal nolisp
setlocal lispoptions=
set list
setlocal list
setlocal makeprg=cargo\ $*
setlocal matchpairs=(:),{:},[:],<:>
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=v:lua.vim.lsp.omnifunc
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
set relativenumber
setlocal relativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal scrollback=-1
setlocal noscrollbind
setlocal shiftwidth=4
set signcolumn=no
setlocal signcolumn=no
setlocal smartindent
setlocal nosmoothscroll
setlocal softtabstop=4
setlocal nospell
setlocal spellcapcheck=
setlocal spellfile=~/.vim/spell/my.utf-8.add
setlocal spelllang=en
setlocal spelloptions=
setlocal statuscolumn=
setlocal suffixesadd=.rs
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'rust'
setlocal syntax=rust
endif
setlocal tabstop=4
setlocal tagfunc=v:lua.vim.lsp.tagfunc
setlocal textwidth=99
setlocal undofile
setlocal varsofttabstop=
setlocal vartabstop=
setlocal winblend=0
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
set nowrap
setlocal nowrap
setlocal wrapmargin=0
let s:l = 396 - ((19 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 396
normal! 0
wincmd w
argglobal
if bufexists(fnamemodify("src/builtin.rs", ":p")) | buffer src/builtin.rs | else | edit src/builtin.rs | endif
if &buftype ==# 'terminal'
  silent file src/builtin.rs
endif
balt src/interp.rs
onoremap <buffer> <silent> [[ :call rust#Jump('o', 'Back')
xnoremap <buffer> <silent> [[ :call rust#Jump('v', 'Back')
nnoremap <buffer> <silent> [[ :call rust#Jump('n', 'Back')
onoremap <buffer> <silent> ]] :call rust#Jump('o', 'Forward')
xnoremap <buffer> <silent> ]] :call rust#Jump('v', 'Forward')
nnoremap <buffer> <silent> ]] :call rust#Jump('n', 'Forward')
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal cinoptions=L0,(s,Ws,J1,j1,m1
setlocal cinscopedecls=public,protected,private
setlocal cinwords=for,if,else,while,loop,impl,mod,unsafe,trait,struct,enum,fn,extern,macro
setlocal colorcolumn=
setlocal comments=s0:/*!,ex:*/,s1:/*,mb:*,ex:*/,:///,://!,://
setlocal commentstring=//\ %s
setlocal complete=.,w,b,u,t
setlocal completefunc=
setlocal completeslash=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal cursorlineopt=both
setlocal nodiff
setlocal errorformat=%-G,%-Gerror:\ aborting\ %.%#,%-Gerror:\ Could\ not\ compile\ %.%#,%Eerror:\ %m,%Eerror[E%n]:\ %m,%Wwarning:\ %m,%Inote:\ %m,%C\ %#-->\ %f:%l:%c,%E\ \ left:%m,%C\ right:%m\ %f:%l:%c,%Z,%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:%c:\ %*\\d:%*\\d\ %t%*[^:]:\ %m,%-G%f:%l\ %s,%-G%*[\ ]^,%-G%*[\ ]^%*[~],%-G%*[\ ]...,%-G%\\s%#Downloading%.%#,%-G%\\s%#Checking%.%#,%-G%\\s%#Compiling%.%#,%-G%\\s%#Finished%.%#,%-G%\\s%#error:\ Could\ not\ compile\ %.%#,%-G%\\s%#To\ learn\ more\\,%.%#,%-G%\\s%#For\ more\ information\ about\ this\ error\\,%.%#,%-Gnote:\ Run\ with\ `RUST_BACKTRACE=%.%#,%.%#panicked\ at\ \\'%m\\'\\,\ %f:%l:%c
setlocal expandtab
if &filetype != 'rust'
setlocal filetype=rust
endif
setlocal fixendofline
setlocal foldcolumn=0
set nofoldenable
setlocal nofoldenable
setlocal foldexpr=0
setlocal foldignore=#
set foldlevel=999
setlocal foldlevel=999
setlocal foldmarker={{{,}}}
set foldmethod=marker
setlocal foldmethod=marker
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=1crqnlj
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=\\v^\\s*(pub\\s+)?use\\s+\\zs(\\f|:)+
setlocal includeexpr=rust#IncludeExpr(v:fname)
setlocal indentexpr=GetRustIndent(v:lnum)
setlocal indentkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
set linebreak
setlocal linebreak
setlocal nolisp
setlocal lispoptions=
set list
setlocal list
setlocal makeprg=cargo\ $*
setlocal matchpairs=(:),{:},[:],<:>
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=v:lua.vim.lsp.omnifunc
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
set relativenumber
setlocal relativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal scrollback=-1
setlocal noscrollbind
setlocal shiftwidth=4
set signcolumn=no
setlocal signcolumn=no
setlocal smartindent
setlocal nosmoothscroll
setlocal softtabstop=4
setlocal nospell
setlocal spellcapcheck=
setlocal spellfile=~/.vim/spell/my.utf-8.add
setlocal spelllang=en
setlocal spelloptions=
setlocal statuscolumn=
setlocal suffixesadd=.rs
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'rust'
setlocal syntax=rust
endif
setlocal tabstop=4
setlocal tagfunc=v:lua.vim.lsp.tagfunc
setlocal textwidth=99
setlocal undofile
setlocal varsofttabstop=
setlocal vartabstop=
setlocal winblend=0
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
set nowrap
setlocal nowrap
setlocal wrapmargin=0
let s:l = 127 - ((10 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 127
normal! 0
wincmd w
exe 'vert 1resize ' . ((&columns * 105 + 79) / 158)
exe 'vert 2resize ' . ((&columns * 52 + 79) / 158)
tabnext
edit src/format.rs
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 79 + 79) / 158)
exe 'vert 2resize ' . ((&columns * 78 + 79) / 158)
argglobal
balt src/parse.rs
onoremap <buffer> <silent> [[ :call rust#Jump('o', 'Back')
xnoremap <buffer> <silent> [[ :call rust#Jump('v', 'Back')
nnoremap <buffer> <silent> [[ :call rust#Jump('n', 'Back')
onoremap <buffer> <silent> ]] :call rust#Jump('o', 'Forward')
xnoremap <buffer> <silent> ]] :call rust#Jump('v', 'Forward')
nnoremap <buffer> <silent> ]] :call rust#Jump('n', 'Forward')
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal cinoptions=L0,(s,Ws,J1,j1,m1
setlocal cinscopedecls=public,protected,private
setlocal cinwords=for,if,else,while,loop,impl,mod,unsafe,trait,struct,enum,fn,extern,macro
setlocal colorcolumn=
setlocal comments=s0:/*!,ex:*/,s1:/*,mb:*,ex:*/,:///,://!,://
setlocal commentstring=//\ %s
setlocal complete=.,w,b,u,t
setlocal completefunc=
setlocal completeslash=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal cursorlineopt=both
setlocal nodiff
setlocal errorformat=%-G,%-Gerror:\ aborting\ %.%#,%-Gerror:\ Could\ not\ compile\ %.%#,%Eerror:\ %m,%Eerror[E%n]:\ %m,%Wwarning:\ %m,%Inote:\ %m,%C\ %#-->\ %f:%l:%c,%E\ \ left:%m,%C\ right:%m\ %f:%l:%c,%Z,%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:%c:\ %*\\d:%*\\d\ %t%*[^:]:\ %m,%-G%f:%l\ %s,%-G%*[\ ]^,%-G%*[\ ]^%*[~],%-G%*[\ ]...,%-G%\\s%#Downloading%.%#,%-G%\\s%#Checking%.%#,%-G%\\s%#Compiling%.%#,%-G%\\s%#Finished%.%#,%-G%\\s%#error:\ Could\ not\ compile\ %.%#,%-G%\\s%#To\ learn\ more\\,%.%#,%-G%\\s%#For\ more\ information\ about\ this\ error\\,%.%#,%-Gnote:\ Run\ with\ `RUST_BACKTRACE=%.%#,%.%#panicked\ at\ \\'%m\\'\\,\ %f:%l:%c
setlocal expandtab
if &filetype != 'rust'
setlocal filetype=rust
endif
setlocal fixendofline
setlocal foldcolumn=0
set nofoldenable
setlocal nofoldenable
setlocal foldexpr=0
setlocal foldignore=#
set foldlevel=999
setlocal foldlevel=999
setlocal foldmarker={{{,}}}
set foldmethod=marker
setlocal foldmethod=marker
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=1crqnlj
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=\\v^\\s*(pub\\s+)?use\\s+\\zs(\\f|:)+
setlocal includeexpr=rust#IncludeExpr(v:fname)
setlocal indentexpr=GetRustIndent(v:lnum)
setlocal indentkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
set linebreak
setlocal linebreak
setlocal nolisp
setlocal lispoptions=
set list
setlocal list
setlocal makeprg=cargo\ $*
setlocal matchpairs=(:),{:},[:],<:>
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=v:lua.vim.lsp.omnifunc
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
set relativenumber
setlocal relativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal scrollback=-1
setlocal noscrollbind
setlocal shiftwidth=4
set signcolumn=no
setlocal signcolumn=no
setlocal smartindent
setlocal nosmoothscroll
setlocal softtabstop=4
setlocal nospell
setlocal spellcapcheck=
setlocal spellfile=~/.vim/spell/my.utf-8.add
setlocal spelllang=en
setlocal spelloptions=
setlocal statuscolumn=
setlocal suffixesadd=.rs
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'rust'
setlocal syntax=rust
endif
setlocal tabstop=4
setlocal tagfunc=v:lua.vim.lsp.tagfunc
setlocal textwidth=99
setlocal undofile
setlocal varsofttabstop=
setlocal vartabstop=
setlocal winblend=0
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
set nowrap
setlocal nowrap
setlocal wrapmargin=0
let s:l = 88 - ((0 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 88
normal! 047|
wincmd w
argglobal
if bufexists(fnamemodify("src/format.rs", ":p")) | buffer src/format.rs | else | edit src/format.rs | endif
if &buftype ==# 'terminal'
  silent file src/format.rs
endif
balt src/parse.rs
onoremap <buffer> <silent> [[ :call rust#Jump('o', 'Back')
xnoremap <buffer> <silent> [[ :call rust#Jump('v', 'Back')
nnoremap <buffer> <silent> [[ :call rust#Jump('n', 'Back')
onoremap <buffer> <silent> ]] :call rust#Jump('o', 'Forward')
xnoremap <buffer> <silent> ]] :call rust#Jump('v', 'Forward')
nnoremap <buffer> <silent> ]] :call rust#Jump('n', 'Forward')
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal cinoptions=L0,(s,Ws,J1,j1,m1
setlocal cinscopedecls=public,protected,private
setlocal cinwords=for,if,else,while,loop,impl,mod,unsafe,trait,struct,enum,fn,extern,macro
setlocal colorcolumn=
setlocal comments=s0:/*!,ex:*/,s1:/*,mb:*,ex:*/,:///,://!,://
setlocal commentstring=//\ %s
setlocal complete=.,w,b,u,t
setlocal completefunc=
setlocal completeslash=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal cursorlineopt=both
setlocal nodiff
setlocal errorformat=%-G,%-Gerror:\ aborting\ %.%#,%-Gerror:\ Could\ not\ compile\ %.%#,%Eerror:\ %m,%Eerror[E%n]:\ %m,%Wwarning:\ %m,%Inote:\ %m,%C\ %#-->\ %f:%l:%c,%E\ \ left:%m,%C\ right:%m\ %f:%l:%c,%Z,%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:%c:\ %*\\d:%*\\d\ %t%*[^:]:\ %m,%-G%f:%l\ %s,%-G%*[\ ]^,%-G%*[\ ]^%*[~],%-G%*[\ ]...,%-G%\\s%#Downloading%.%#,%-G%\\s%#Checking%.%#,%-G%\\s%#Compiling%.%#,%-G%\\s%#Finished%.%#,%-G%\\s%#error:\ Could\ not\ compile\ %.%#,%-G%\\s%#To\ learn\ more\\,%.%#,%-G%\\s%#For\ more\ information\ about\ this\ error\\,%.%#,%-Gnote:\ Run\ with\ `RUST_BACKTRACE=%.%#,%.%#panicked\ at\ \\'%m\\'\\,\ %f:%l:%c
setlocal expandtab
if &filetype != 'rust'
setlocal filetype=rust
endif
setlocal fixendofline
setlocal foldcolumn=0
set nofoldenable
setlocal nofoldenable
setlocal foldexpr=0
setlocal foldignore=#
set foldlevel=999
setlocal foldlevel=999
setlocal foldmarker={{{,}}}
set foldmethod=marker
setlocal foldmethod=marker
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=1crqnlj
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=\\v^\\s*(pub\\s+)?use\\s+\\zs(\\f|:)+
setlocal includeexpr=rust#IncludeExpr(v:fname)
setlocal indentexpr=GetRustIndent(v:lnum)
setlocal indentkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
set linebreak
setlocal linebreak
setlocal nolisp
setlocal lispoptions=
set list
setlocal list
setlocal makeprg=cargo\ $*
setlocal matchpairs=(:),{:},[:],<:>
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=v:lua.vim.lsp.omnifunc
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
set relativenumber
setlocal relativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal scrollback=-1
setlocal noscrollbind
setlocal shiftwidth=4
set signcolumn=no
setlocal signcolumn=no
setlocal smartindent
setlocal nosmoothscroll
setlocal softtabstop=4
setlocal nospell
setlocal spellcapcheck=
setlocal spellfile=~/.vim/spell/my.utf-8.add
setlocal spelllang=en
setlocal spelloptions=
setlocal statuscolumn=
setlocal suffixesadd=.rs
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'rust'
setlocal syntax=rust
endif
setlocal tabstop=4
setlocal tagfunc=v:lua.vim.lsp.tagfunc
setlocal textwidth=99
setlocal undofile
setlocal varsofttabstop=
setlocal vartabstop=
setlocal winblend=0
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
set nowrap
setlocal nowrap
setlocal wrapmargin=0
let s:l = 139 - ((20 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 139
normal! 025|
wincmd w
2wincmd w
exe 'vert 1resize ' . ((&columns * 79 + 79) / 158)
exe 'vert 2resize ' . ((&columns * 78 + 79) / 158)
tabnext
edit src/main.rs
argglobal
balt NOTE.md
onoremap <buffer> <silent> [[ :call rust#Jump('o', 'Back')
xnoremap <buffer> <silent> [[ :call rust#Jump('v', 'Back')
nnoremap <buffer> <silent> [[ :call rust#Jump('n', 'Back')
onoremap <buffer> <silent> ]] :call rust#Jump('o', 'Forward')
xnoremap <buffer> <silent> ]] :call rust#Jump('v', 'Forward')
nnoremap <buffer> <silent> ]] :call rust#Jump('n', 'Forward')
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal cinoptions=L0,(s,Ws,J1,j1,m1
setlocal cinscopedecls=public,protected,private
setlocal cinwords=for,if,else,while,loop,impl,mod,unsafe,trait,struct,enum,fn,extern,macro
setlocal colorcolumn=
setlocal comments=s0:/*!,ex:*/,s1:/*,mb:*,ex:*/,:///,://!,://
setlocal commentstring=//\ %s
setlocal complete=.,w,b,u,t
setlocal completefunc=
setlocal completeslash=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal cursorlineopt=both
setlocal nodiff
setlocal errorformat=%-G,%-Gerror:\ aborting\ %.%#,%-Gerror:\ Could\ not\ compile\ %.%#,%Eerror:\ %m,%Eerror[E%n]:\ %m,%Wwarning:\ %m,%Inote:\ %m,%C\ %#-->\ %f:%l:%c,%E\ \ left:%m,%C\ right:%m\ %f:%l:%c,%Z,%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:%c:\ %*\\d:%*\\d\ %t%*[^:]:\ %m,%-G%f:%l\ %s,%-G%*[\ ]^,%-G%*[\ ]^%*[~],%-G%*[\ ]...,%-G%\\s%#Downloading%.%#,%-G%\\s%#Checking%.%#,%-G%\\s%#Compiling%.%#,%-G%\\s%#Finished%.%#,%-G%\\s%#error:\ Could\ not\ compile\ %.%#,%-G%\\s%#To\ learn\ more\\,%.%#,%-G%\\s%#For\ more\ information\ about\ this\ error\\,%.%#,%-Gnote:\ Run\ with\ `RUST_BACKTRACE=%.%#,%.%#panicked\ at\ \\'%m\\'\\,\ %f:%l:%c
setlocal expandtab
if &filetype != 'rust'
setlocal filetype=rust
endif
setlocal fixendofline
setlocal foldcolumn=0
set nofoldenable
setlocal nofoldenable
setlocal foldexpr=0
setlocal foldignore=#
set foldlevel=999
setlocal foldlevel=999
setlocal foldmarker={{{,}}}
set foldmethod=marker
setlocal foldmethod=marker
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=1crqnlj
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=\\v^\\s*(pub\\s+)?use\\s+\\zs(\\f|:)+
setlocal includeexpr=rust#IncludeExpr(v:fname)
setlocal indentexpr=GetRustIndent(v:lnum)
setlocal indentkeys=0{,0},!^F,o,O,0[,0],0(,0)
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
set linebreak
setlocal linebreak
setlocal nolisp
setlocal lispoptions=
set list
setlocal list
setlocal makeprg=cargo\ $*
setlocal matchpairs=(:),{:},[:],<:>
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=v:lua.vim.lsp.omnifunc
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
set relativenumber
setlocal relativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal scrollback=-1
setlocal noscrollbind
setlocal shiftwidth=4
set signcolumn=no
setlocal signcolumn=no
setlocal smartindent
setlocal nosmoothscroll
setlocal softtabstop=4
setlocal nospell
setlocal spellcapcheck=
setlocal spellfile=~/.vim/spell/my.utf-8.add
setlocal spelllang=en
setlocal spelloptions=
setlocal statuscolumn=
setlocal suffixesadd=.rs
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'rust'
setlocal syntax=rust
endif
setlocal tabstop=4
setlocal tagfunc=v:lua.vim.lsp.tagfunc
setlocal textwidth=99
setlocal undofile
setlocal varsofttabstop=
setlocal vartabstop=
setlocal winblend=0
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
set nowrap
setlocal nowrap
setlocal wrapmargin=0
let s:l = 61 - ((17 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 61
normal! 044|
tabnext 2
set stal=1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
set shortmess=ltToOCF
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
