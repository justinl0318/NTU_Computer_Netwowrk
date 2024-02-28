# Log Checker

The main script is `log_test.py`. 

```
Test logs against the specification.

The below tests are PROBABLY NOT used in the real tests, but if you want to check that
you have a set of plausible logs, YOU SHOULD PASS THOSE TESTS.

Real, autograde tests are not in here. They're in another file that is not given to students.

When you finish an experiment, do the following to check the result: 
    $ python log_test.py <send_log.txt> <recv_log.txt> <agent_log.txt> <src_filepath> <dst_filepath>

The text is colored by default. If you save the output as a file `op` and want to read, you can try:
    $ cat op        # tty deals with colors natually
    $ less -R op    # less cannot deal with colors without -R
    + Use vscode extensions, e.g. "ANSI Colors": command prompt "ANSI Text: Open Preview"
    + Modify some functions below to return non-colored text

To remove colors / convert to pure text, use something like this
    $ python ...  | sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,2};?)?)?[mGK]//g"
```