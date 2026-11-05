![C23](https://img.shields.io/badge/C-C23-blue?style=flat-square)
![SDL2](https://img.shields.io/badge/SDL2-required-green?style=flat-square)
![SDL2_ttf](https://img.shields.io/badge/SDL2__ttf-required-green?style=flat-square)

![version](https://img.shields.io/badge/version-v1.0.0-orange?style=flat-square)

# Clhart
graphing software/language

## Installation and file extension:

**Option 1: Clone the repo**

1. Clone the repo to `C:\Clhart`
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

## Usage:

### writing ranges:

to write a range you should write the minimumn desired value then a semicolon (`;`) and the maximum desired value

### writing comments:

you can make a comment in 3 ways:

1. starting a line with `<>`
2. starting a line with `//`
3. starting a line with `#`

### Global parameters and directives

#### Directives
- `.setup {}` changes graph settings
- `.title {[title]}` adds a title
- `.type {}` specifies the graph type

#### Setup parameters
- `show_labels` displays identifiers instead of names
- `legend` displays a color-coded legend

### graph types:

#### histogram:
  
- its type specific setup parameters are:
    * x_range 
    * y_range
    * show_grid

- it can be switched from fuzzy to distribution using `.switch`

##### Fuzzy:

- its data is written like the following:

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
    [parameter display name] --- range start;range end|amount :color:,
}
```

#### nodes:

- their type specific setup parameters are:
    * start_angle
    * advance_angle

- to connect nodes you can do either `<->`, `<--` or `-->` to change the way they connect
- to make a node write its display name then `---` and right after an optional identifier and/or color

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
    * show_precentaeges
    * three_dimensional

- to make a slice write its name then `---` then the amount of the slice and then an optional color

- egg sample:

```text
slices {
    [slice name] --- [amount] :[optional hex color]:,
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