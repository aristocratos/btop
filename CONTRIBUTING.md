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

## Optimization

* Avoid writing to disk if possible.

* Make sure variables/vectors/maps/classes etc. are cleaned up if not reused.

* Compare cpu and memory usage with and without your code and look for alternatives if they cause a noticeable negative impact.

For questions contact Aristocratos at admin@qvantnet.com

For proposing changes to this document create a [new issue](https://github.com/aristocratos/btop/issues/new/choose).
