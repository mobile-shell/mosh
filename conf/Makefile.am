completionsdir = @completions@
dist_completions_DATA =
nobase_dist_sysconf_DATA =

if INSTALL_UFW
  nobase_dist_sysconf_DATA += ufw/applications.d/mosh
endif

if INSTALL_COMPLETION
  dist_completions_DATA += bash-completion/completions/mosh
endif
