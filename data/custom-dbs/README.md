# Custom databases

The zips here go in `Seasons/Custom/` of each download (`tools/release/make_release.sh`, the default of
`--custom-dbs`). The game unpacks each once into `Databases` of the save folder, and the Choose Season screen lists
it on the Custom tile.

- `2026 Preview Data Update.zip`: the CM 01/02 Update Team's 2026 Data Update preview
  ([champman0102.co.uk](https://champman0102.co.uk)), turned from their preview save into a database folder and
  merged with May 2026 by `tools/save/preview_db/build_preview_db.py`, then zipped by
  `tools/release/make_custom_db_zip.py`. How it is made: `docs/research/2026-10-26-database-from-save.md`.
