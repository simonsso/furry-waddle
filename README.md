# Furry-waddle
This will parse output from Swedish net stock broker Avanza and perform basic filtering on ISIN create reports.

But mostly I use it to show how the data structure and implementation impacts execution times.

## Status: 2019-02-11
- reads csv
  - read full and
  - incremental read
- Transaction index
  - All stored in sorted deque ledger
  - For every security an index is creased on ISIN pointing to data in ledger (sorted on date)
  - A date index is created linking the first transaction of a day in ledger
- example searches and calculations
  - sum the amount and brokerage for all trades in one isin
  - sum based on time.
- Network interface and multi threading
  - Each incoming connection is handled in separate thread


## CI: Travis build status
[![Build Status](https://travis-ci.org/simonsso/furry-waddle.svg?branch=master)](https://travis-ci.org/simonsso/furry-waddle)
