/**
 * @file
 * Shared constants/structs that are private to Compression
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUTT_COMPRESS_COMPRESS_PRIVATE_H
#define MUTT_COMPRESS_COMPRESS_PRIVATE_H

#define COMPRESS_OPS(_name)                         \
  const struct ComprOps compr_##_name##_ops = {     \
    .name       = #_name,                           \
    .open       = compr_##_name##_open,             \
    .compress   = compr_##_name##_compress,         \
    .decompress = compr_##_name##_decompress,       \
    .close      = compr_##_name##_close,            \
  };

#endif /* MUTT_COMPRESS_COMPRESS_PRIVATE_H */
