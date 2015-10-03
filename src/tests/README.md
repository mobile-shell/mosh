#
# This is an end-to-end test for mosh.  It works by generating stimuli
# for mosh, capturing its generated screens with tmux, comparing the
# resulting captures, and of course checking exit statuses.
#
# In accordance with GNU Automake, this script returns these exit
# status values:
#
# 0 test success
# 1 test failure
# 77 test skipped (tmux or ssh is unavailable if needed)
# 99 hard error
#

#
# This test is structured as 3 different scripts:
# <testname>.test, the actual test code
# e2e-test, the meat of the test framework
# e2e-test-server-wrapper, a framework script to handle server-side test execution
#
# <testname>.test is a multirole script.  If invoked with no
# arguments, it should simply invoke e2e-test <testname>.test
# <test-actions>.  If invoked with an argument (one of the test-action
# names given as arguments by the no-argument form), it performs some
# part of the actual test.
#
# e2e-test is the heart of this test system.  It runs multiple
# actions, logs their output, compares their results, and generates
# the final result (exitstatus, mostly) for the Automake testing
# framework.  Each action is run, and recorded in
# <testname>.test.d/<run>.*
# <run>.exitstatus is the exitstatus from the server wrapper.
# <run>.tmux.log is the output of tmux for the entire test run
# <run>.capture is a capture of the Mosh client screen after the test
#  action is complete, generated with tmux capture-pane.
#
# The possible test-actions are:
# baseline
# same
# different
# client
#
# The first three of these are tests to be run on the server.  Simple
# ones will just output some text to the console After the test is
# run, the tmux screen will be captured from the client mosh.  The
# `baseline` test is always run twice: once without mosh and once
# with.  The `different` test is expected to use some slightly
# different stimuli to mosh that we expect will cause mosh to generate
# different output.  A sample use case for this might be a regression
# test for a new escape sequence handler.  The `same` test is also
# expected to use slightly different stimuli to mosh that we expect
# will cause mosh to generate the *same* output.  A sample use case
# might be code that validates Mosh's handling of edge cases involving
# Unicode.  The `client` test is simply an invocation of Mosh that
# replaces e2e-test's default, and might be used to test Mosh's
# network and/or SSH handling.
#
# As noted above, `baseline` actually does two test runs, one called
# `baseline`, with mosh, and one called `direct`, without mosh.  Both
# invoke the test script with action name `baseline`.  The screen
# captures of these two runs are compared.
#
# The `same` action runs once and is compared with `baseline`.  If
# they compare identical, the test succeeds.
#
# The `different` action runs once and is compared with `baseline`.
# If they compare different, the test succeeds.
#


#
# tmux is run in command mode, always with its default size 80x24, as
# `tmux -C new-session test args...`  This generates a printable
# output captured into <run>.tmux.log.
#

