# Contributing

Keep the library deterministic and self-contained. New job commands must use `ByteCursor`, cap collection sizes before allocation, and leave malformed commands as no-ops. Avoid filesystem, network, locale, clock, and thread dependencies in replay code.

Add a valid corpus job when introducing a new state transition. A useful seed reaches the subsystem with coherent IDs and resources but does not itself fail. Build all four fuzz targets after changing shared models or ownership behavior.
