# VGA has strange color

## Scope
* All platforms
* Git `f17a7235e`

## Behavior
VGA has strange color when a new line is printed and the entire screen scrolls
up.

## Debugging
After reviewing code, `vgamem_newln()` uses two `memcpy()` calls. The first one
should ideally be `memmove()`; the second one should be `memset()`. The second
one is the root cause of the unexpceted behavior.

## Fix

`f17a7235e..da97a50e1`
* Fix unexpected content on screen when debugging with VGA

