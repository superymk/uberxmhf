# XMHF boot sequence
init_start (entry) ->
cstartup
 print banner (AMT 3-4)
 detect Intel / AMD CPU (AMT 7)
 dealwithMP: TODO
