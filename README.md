![C23](https://img.shields.io/badge/C-C23-blue?style=flat-square)
![SDL2](https://img.shields.io/badge/SDL2-required-green?style=flat-square)
![SDL2_ttf](https://img.shields.io/badge/SDL2_ttf-required-green?style=flat-square)

![version](https://img.shields.io/badge/version-v1.3.0-orange?style=flat-square)

# Clhart
graphing software/language

## Installation and file extension:

1. download the `.exe` from the repo
   * If the required SDL2 DLLs (egg. `SDL2_ttf.dll`) are not in **your system PATH**, download them from somewhere

2. Add `C:\Clhart` to **PATH**[^first]

## Quick Start:

1. Create a file called `countries.clh`:

2. write the following inside:

```clh
.type {nodes}

.setup {
    start_angle --- 0,
    advance_angle --- 20
}

nodes {
    Europe --- EU :#0000FF:,
    North_America --- NA :#FFA500:,
    Africa --- AF :#FFffFF:,

    NA <-- EU,
    AF <-> EU,
    AF <-> NA
}
```

3. run the file with clhart.exe

Press **Ctrl+S** while viewing a chart to save a PNG at twice the window's
resolution in the same folder as the `.clh` file. The filename uses the chart
title, or the `.clh` filename when there is no title. If that PNG already exists,
a number is added to the filename.

## Usage:

### writing ranges:

to write a range you should write the minimumn desired value then a semicolon (`;`) and the maximum desired value

### writing comments:

you can make a comment in 2 ways:

1. starting a line with `<>`
2. starting a line with `//`

### Global parameters and directives

#### Directives
- `.setup {}` changes graph settings
- `.title {[title]}` adds a title
- `.type {}` specifies the graph type
- `.images {}` loads named images for use in charts

Declare each image name and file path in `.images`; then refer to it in chart data with `"*)image name(*"`:

```text
.images {
    "image name" --- (*"images/picture.png"*)
}
```

Relative image paths are resolved from the directory containing the `.clh` file.

#### Setup parameters
- `show_labels` displays identifiers instead of names
- `legend` displays a color-coded legend
- `no_underscore_names` makes all display names render spaces instead of underscores (`_`)
- `xlabel` and `ylabel` set the horizontal and vertical axis labels on histogram, bar, column, and line charts. Use quotes for labels with spaces or commas. Combination charts set these in each child chart's `.setup`.

```text
.setup {
    xlabel --- "Year, reported",
    ylabel --- "Population"
}
```

#### Misc

- a data object can reference a named image with `"*)image name(*"`; the image is drawn to the data

### graph types:

#### histogram:
  
- its type specific setup parameters are:
    * x_range 
    * y_range
    * show_grid

- it can be switched from fuzzy to distribution using `.switch`

##### Fuzzy:

- its data is written like the following:
- fuzzy parameters do not support images

```text
parameters {
    [parameter display name] --- starting value;mid value;end value :color:,
    [parameter display name] --- starting value;mid start value;mid end value;end value :color:
}
```

##### Distribution

- its data is written like the following:

```text
parameters {
    [parameter display name] --- range start;range end|amount "*)image name(*" :color:,
}
```

- distribution parameter images are scaled down to fit inside their bars; image references and colors are optional and can appear in either order

#### nodes:

- their type specific setup parameters are:
    * start_angle
    * advance_angle

- to connect nodes you can do either `<->`, `<--` or `-->` to change the way they connect
- to make a node write its display name then `---` and right after an optional identifier, image reference, and/or color

- egg sample:

```text
nodes {
    [node display name] --- [optional node identifier] :[optional node hex color]:,

    [node identifier or name] <-> [node identifier or name],
    [node identifier or name] <-- [node identifier or name],
    [node identifier or name] --> [node identifier or name]
}
```

#### pie:

- its specific setup parameters are:
    * show_precentages
    * three_dimensional

- to make a slice write its name then `---` then the amount, followed by an optional image reference and/or color
- a slice image is scaled down to fit inside the slice while preserving its aspect ratio

- egg sample:

```text
slices {
    [slice name] --- [amount] "*)image name(*" :[optional hex color]:,
    [slice name] --- [amount] :[optional hex color]:
}
```

#### bar:

- its specific setup parameters are:
    * horizontal
    * height_range
    * show_values
    * show_axis_numbers

- to make a bar write its name then `---` then its value and an optional color

- egg sample:

```text
bars {
    [bar name] --- [value] :[optional hex color]:,
    [bar name] --- [value] :[optional hex color]:
}
```

#### column:

- its specific setup parameters are:
    * show_values
    * show_axis_numbers
    * y_min
    * y_max

- to make a column chart, first define its categories, then define its data series
- category entries may reference images; images belong to categories, not data series, and are scaled to fit below the category on the x-axis
- each series has a name, followed by `---`, its values separated by `;`, and an optional color
- each value corresponds to a category in the same position
- if categories are not specified, they are generated automatically

- egg sample:

```text
categories {
    [category name] "*)image name(*",
    [category name],
    [category name]
}

columns {
    [series name] --- [value];[value];[value] :[optional hex color]:,
    [series name] --- [value];[value];[value] :[optional hex color]:
}
```

#### line:

- line charts use the same `categories` and series syntax as column charts
- category images are attached to the x-axis categories, not to line series
- each series is drawn as a line connecting its values across the categories
- setup parameters are `show_values`, `show_axis_numbers`, and `height_range`

```text
categories {
    [category name],
    [category name],
    [category name]
}

lines {
    [series name] --- [value];[value];[value] :[optional hex color]:,
    [series name] --- [value];[value];[value] :[optional hex color]:
}
```

#### display:

- displays one image directly, without a data block
- the `fit` setup option can be `fit` (preserve aspect ratio and scale to fit), `stretch` (fill the window), or `none` (native size, centered)
- an optional `border` takes a pixel thickness and a hex color
- image paths are relative to the `.clh` file

```text
.type {display}
.setup {
    fit --- fit,
    border --- 2px :#FFFFFF:
}
(*"images/picture.png"*)
```

#### combination:

- its specific setup parameters are
    * show_sub_titles

- to make a sub charts add `{ chart data }` in your main data

- egg sample:

```text
charts {
    {
        .type {pie}
        .title {title}

        .setup {
            three_dimensional --- 1,
            show_percentages --- 1
        }

        slices {
            data
        }
    },
    {
        .type {bar}
        .title {title}

        .setup {
            horizontal --- 1,
            show_axis_numbers --- 1,
            legend --- 1
        }

        bars {
            data
        }
    }
}
```

# Made by:

- [pasztet211](https://github.com/pasztet211) ![pasztet](https://img.shields.io/badge/creator-orange?style=flat-square)

[^first]:here is a guide 🔗[how to add to path](https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/)