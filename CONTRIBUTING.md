# Contributing guidelines

## When submitting pull requests

* Explain your thinking in why a change or addition is needed.
  * Is it a requested change or feature?
  * If not, open a feature request to get feedback before making a pull request.

* Split up multiple unrelated changes in multiple pull requests.

* If it's a fix for a unreported bug, make a bug report and link the pull request.

* Purely cosmetic changes won't be accepted without a very good explanation of its value.

## Formatting

### Follow the current syntax design

* Indent type: Tabs

* Tab size: 4

* Alternative operators `and`, `or` and `not`.

* Opening curly braces `{` at the end of the same line as the statement/condition.

## General guidelines

* Don't force a programming style. Use object oriented, functional, data oriented, etc., where it's suitable.

* Use [RAII](https://en.cppreference.com/w/cpp/language/raii).

* Make use of the standard algorithms library, (re)watch [C++ Seasoning](https://www.youtube.com/watch?v=W2tWOdzgXHA) and [105 STL Algorithms](https://www.youtube.com/watch?v=bFSnXNIsK4A) for inspiration.

* Make use of the included [robin_hood unordered map & set](https://github.com/martinus/robin-hood-hashing)

* Do not add includes if the same functionality can be achieved using the already included libraries.

* Use descriptive names for variables.

* Use comments if not very obvious what your code is doing.

* Add comments as labels for what's currently happening in bigger sections of code for better readability.

* Avoid writing to disk.

* If using the logger functions, be sensible, only call it if something of importance has changed.

* Benchmark your code and look for alternatives if they cause a noticeable negative impact.

For questions open a new discussion thread or send a mail to jakob@qvantnet.com

For proposing changes to this document create a [new issue](https://github.com/aristocratos/btop/issues/new/choose).
