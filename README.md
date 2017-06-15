books
=====

Convert and reproduce order books.


Red tape
--------

- licensed under [BSD3c][1]
- one dependency: a C99 compiler with decimal float support


Motivation
----------

Limit order books are usually buried deep within the software that
operates on them.  Lacking a wide-spread format, limit order books are
typically binary blobs that only the surrounding software can
interpret.

The only two software-invariant formats are either snapshots of a fixed
number of levels or a timestamped order-by-order (or level-by-level)
stream.  Customers are then expected to build up the book as needed
themselves.  This is where `books` sees its place.


Similar projects
================

None.


  [1]: http://opensource.org/licenses/BSD-3-Clause
