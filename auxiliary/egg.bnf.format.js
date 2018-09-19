/*jslint esversion:6*/
/*jslint bitwise:true*/
/*jshint loopfunc:true*/
/*globals egg*/
(function() {
  "use strict";
  var railroad = document.getElementById("railroad");
  var bnf = document.getElementById("bnf");
  var data = null;
  function initialise() {
    var success = true;
    var rules = Object.keys(egg);
    function expand(name, rule) {
      var keys = Object.keys(rule);
      if (keys.length !== 1) {
        console.error("Invalid tags for '" + name + "':", JSON.stringify(keys));
        return false;
      }
      var key = keys[0];
      var value = rule[key];
      switch (key) {
        case "choice":
        case "sequence":
          if (!Array.isArray(value)) {
            console.error("'" + name + "'", key, "expected to be an array");
            return false;
          }
          for (var i in value) {
            switch (typeof value[i]) {
              case "string":
                if (!rules.includes(value[i])) {
                  console.warn("'" + name + "'", key, "has unknown rule: '" + value[i] + "'");
                  return false;
                }
                break;
              case "object":
                if (!expand(name, value[i])) {
                  return false;
                }
                break;
              default:
                console.error("'" + name + "'", key, "has bad element " + i + ":", JSON.stringify(value[i]));
                return false;
            }
          }
          break;
        case "zeroOrOne":
        case "zeroOrMore":
        case "oneOrMore":
          switch (typeof value) {
            case "string":
              if (!rules.includes(value)) {
                console.warn("'" + name + "'", key, "has unknown rule: '" + value + "'");
                return false;
              }
              break;
            case "object":
              if (!expand(name, value)) {
                return false;
              }
              break;
            default:
              console.error("'" + name + "'", key, "has bad value:", JSON.stringify(value));
              return false;
          }
          break;
        case "token":
        case "terminal":
          if (typeof value !== "string") {
            console.error("'" + name + "'", key, "has bad value:", JSON.stringify(value));
            return false;
          }
          break;
        case "tokens":
          if (!Array.isArray(value)) {
            console.error("'" + name + "'", key, "expected to be an array");
            return false;
          }
          for (var j of value) {
            if (typeof j !== "string") {
              console.error("'" + name + "'", key, "has bad element " + i + ":", JSON.stringify(j));
              return false;
            }
          }
          break;
        default:
          console.error("Invalid tag for '" + name + "':", JSON.stringify(key));
          return false;
      }
      return true;
    }
    for (var rule of rules) {
      success &= expand(rule, egg[rule]);
      egg[rule].name = rule;
    }
    data = egg[rules[0]];
    return success;
  }
  function output() {
    var rules = Object.keys(egg);
    function expand(rule, parentheses) {
      if (typeof rule === "string") {
        return rule;
      }
      var key = Object.keys(rule)[0];
      var value = rule[key];
      switch (key) {
        case "sequence":
          if (parentheses) {
            return "(" + value.map(expand).join(" ") + ")";
          }
          return value.map(expand).join(" ");
        case "zeroOrOne":
          return expand(value, true) + "?";
        case "zeroOrMore":
          return expand(value, true) + "*";
        case "oneOrMore":
          return expand(value, true) + "+";
        case "token":
          return "'" + value + "'";
        case "terminal":
          return "[" + value + "]";
        case "tokens":
          return value.map(x => "'" + x + "'").join(" | ");
      }
      console.error("Invalid tag:", JSON.stringify(key));
      return "<unknown>";
    }
    var text = [];
    for (var rule of rules) {
      if (egg[rule].choice) {
        var lines = "";
        var before = rule + " ::= ";
        for (var choice of egg[rule].choice) {
          lines += before + expand(choice);
          before = "\n" + " ".repeat(rule.length + 3) + "| ";
        }
        text.push(lines);
      } else {
        text.push(rule + " ::= " + expand(egg[rule]));
      }
    }
    bnf.textContent += text.join("\n\n");
  }
  function layout_xy() {
    var layout = {
      "FORTRAN": [89, 0, 2, +0.5],
      "ALGOL": [111, 1, 2],
      "Lisp": [219, 1, 8, +0.5],
      "COBOL": [58, 2],
      "SNOBOL": [297, 8, 12, +0.5],
      "CPL": [51, 9],
      "BASIC": [319, 14, 82],
      "PL/I": [60, 14, 4, -0.5],
      "Simula": [136, 16, 9],
      "APL": [6, 17, 1, +0.5],
      "BCPL": [51, 18],
      "Logo": [232, 20, 4],
      "B": [52, 24],
      "Scheme": [209, 25, 2],
      "Pascal": [121, 25, 8],
      "C": [87, 33, 3],
      "Prolog": [268, 33, 7, +0.5],
      "Smalltalk": [223, 33, 10],
      "ML": [223, 52, 1],
      "CLU": [76, 53, 4],
      "Modula": [121, 53, 7],
      "Shell Scripts": [304, 58, 0, +0.5],
      "AWK": [299, 64],
      "CSP": [23, 65, 1],
      "Rexx": [130, 66],
      "Ada": [115, 67],
      "MATLAB": [5, 69, 3],
      "Objective-C": [79, 69, 24],
      "C++": [104, 72, 0, 1],
      "Miranda": [223, 72, 6],
      "Newsqueak": [35, 72],
      "Object Pascal": [133, 82, 2],
      "Erlang": [255, 82],
      "Oberon": [122, 82, 6, -0.5],
      "Eiffel": [169, 82, 4],
      "Perl": [281, 85, 4],
      "Tcl": [294, 88, 8],
      "Haskell": [237, 97],
      "Python": [71, 104],
      "Oz": [263, 105, 3],
      "Visual Basic": [310, 105, 7],
      "Dylan": [203, 110],
      "Alef": [45, 110, 7],
      "Lua": [185, 111, 2],
      "R": [217, 111],
      "Java": [159, 119],
      "JavaScript": [175, 126, 0, -0.5],
      "Limbo": [29, 126, 7],
      "PHP": [288, 126],
      "Ruby": [192, 126, 15],
      "OCaml": [231, 127, 6],
      "ActionScript": [120, 132],
      "Alice": [268, 133],
      "C#": [100, 133, 1],
      "D": [93, 141, 3],
      "Visual Basic .NET": [317, 141],
      "Scratch": [274, 142],
      "Scala": [245, 143, 3],
      "PowerShell": [300, 144],
      "Clojure": [149, 149],
      "Go": [15, 150],
      "Rust": [51, 151],
      "Dart": [209, 152],
      "Kotlin": [167, 152],
      "Julia": [0, 153],
      "TypeScript": [133, 153],
      "Swift": [64, 156, 0, -0.5],
      "egg": [111, 157]
    };
    var y = 0;
    data.forEach(i => {
      var xy = layout[i.name];
      console.assert(xy);
      y += xy[1];
      i.x = xy[0];
      i.y = xy[1];
      i.elbow = xy[2] || 0;
      i.nudge = xy[3] || 0;
    });
  }
  function half(x, phase) {
    return (phase === 0) ? (x * 0.5) : (x >> 1);
  }
  function incoming_xy(to, from, phase) {
    var x = to.x + half(to.width - to.incoming.length, phase);
    if (from) {
      x += to.incoming.findIndex(x => x.from === from);
    }
    if (phase === 0) {
      x += to.nudge;
    }
    return { x: x, y: to.y };
  }
  function outgoing_xy(from, to, phase) {
    var x = from.x + half(from.width - from.outgoing.length, phase);
    if (to) {
      x += from.outgoing.findIndex(x => x.to === to);
    }
    if (phase === 0) {
      x += from.nudge;
    }
    return { x: x, y: from.y + 3 };
  }
  function elbow_y(from, to) {
    var bend = from.outgoing.find(x => x.to === to).bend || 0;
    return from.y + from.elbow + Math.abs(bend) + 4;
  }
  function layout_connectors() {
    var i, j, ox, left, right;
    for (i of data) {
      i.outgoing.sort((a, b) => a.to.x - b.to.x);
    }
    for (i of data) {
      if (i.incoming.length) {
        var i0 = incoming_xy(i).x;
        var i1 = i0 + i.incoming.length - 1;
        left = [];
        var middle = [];
        right = [];
        for (j of i.incoming) {
          ox = outgoing_xy(j.from, i).x;
          if (ox < i0) {
            left.push(j);
          } else if (ox > i1) {
            right.push(j);
          } else {
            middle.push(j);
          }
        }
        console.assert(middle.length <= 1);
        if (middle.length) {
          ox = outgoing_xy(middle[0].from, i).x;
          if (ox < (i0 + left.length)) {
            left.push(middle[0]);
            middle = [];
          } else if (ox > (i1 - right.length)) {
            right.push(middle[0]);
            middle = [];
          }
        }
        left.sort((a, b) => elbow_y(b.from, i) - elbow_y(a.from, i));
        right.sort((a, b) => elbow_y(a.from, i) - elbow_y(b.from, i));
        i.incoming = [...left, ...middle, ...right];
      }
    }
    for (var from of data) {
      left = 0;
      right = 0;
      for (j of from.outgoing) {
        var to = j.to;
        var ix = incoming_xy(to, from).x;
        ox = outgoing_xy(from, to).x;
        if (ix < ox) {
          j.bend = left--;
        } else if (ix > ox) {
          right++;
        } else {
          j.bend = 0;
        }
      }
      from.maxbend = Math.max(-left, right);
      for (j of from.outgoing) {
        if (!("bend" in j)) {
          j.bend = --right;
        }
      }
    }
  }
  function element(name, attributes, content) {
    var result = "<" + name;
    for (var i of Object.keys(attributes)) {
      result += " " + i + "=\"" + attributes[i] + "\"";
    }
    if (content) {
      result += ">" + content + "</" + name + ">";
    } else {
      result += "/>";
    }
    return result;
  }
  function link(href, content) {
    return element("a", { href: href, target: "_blank" }, content);
  }
  var flow = [];
  var svg = "";
  var scale_y = 1.5;
  function set(x, y, text) {
    while (flow.length <= y) {
      flow.push("");
    }
    if (flow[y].length <= x) {
      flow[y] += " ".repeat(x - flow[y].length) + text;
    } else {
      flow[y] = flow[y].slice(0, x) + text + flow[y].slice(x + text.length);
    }
  }
  function centre(text, width) {
    var pad = width - text.length;
    console.assert(pad > 0);
    return " ".repeat(pad >> 1) + text + " ".repeat(pad - (pad >> 1));
  }
  function hsla(i, lightness, opacity) {
    var egginess = i.index / (COCL.length - 1);
    var hue = (30 + 4321  * (1 - egginess)) % 360;
    return "hsla(" + hue + ",100%," + lightness + "%," + opacity + "%)";
  }
  function label(i) {
    // TXT
    var x, y = i.y;
    var h, w = i.width - 2;
    set(i.x, y, "╔" + "═".repeat(w) + "╗");
    set(incoming_xy(i).x, y, "╧".repeat(i.incoming.length));
    set(i.x, ++y, "║" + centre(i.name, w) + "║");
    if (i.year) {
      set(i.x, ++y, "║" + centre(i.year.toString(), w) + "║");
    }
    set(i.x, ++y, "╚" + "═".repeat(w) + "╝");
    set(outgoing_xy(i).x, y, "╤".repeat(i.outgoing.length));
    // SVG
    var light = hsla(i, 90, 100);
    var dark = hsla(i, 20, 100);
    w = i.width;
    h = (i.year ? 3 : 2.2) * scale_y;
    x = i.x + i.nudge - 0.5;
    y = i.y * scale_y;
    var part = element("rect", { x: x, y: y, width: w, height: h, fill: light, stroke: dark, "stroke-width": 0.1, rx: 0.5, ry: 0.5 });
    x += w * 0.5;
    y += 1.35 * scale_y;
    part += element("text", { x: x, y: y, "font-size": 2, "font-weight": "bold", "text-anchor": "middle", fill: dark }, i.name);
    if (i.year) {
      y += 1.2 * scale_y;
      part += element("text", { x: x, y: y, "font-size": 1.7, "text-anchor": "middle", fill: dark }, i.year);
    }
    svg += link("https://en.wikipedia.org/wiki/" + i.wiki, part);
  }
  function horizontal(x, y, n) {
    console.assert(n >= 0);
    set(x, y, "─".repeat(n));
  }
  function vertical(x, y, n) {
    console.assert(n >= 0);
    for (var i = 0; i < n; ++i) {
      set(x, y + i, "│");
    }
  }
  function bendAngle(dx, r) {
    console.assert(dx > 0);
    return Math.acos(1 - dx / (r + 1));
  }
  function bendLeft(x0, y0, ym, x1, y1, r) {
    if (x1 >= (x0 - r)) {
      var a = bendAngle(x0 - x1, r);
      var xa = x0 + r * (Math.cos(a) - 1);
      var ya = ym + r * (Math.sin(a) - 1);
      var yd = ya + Math.sin(a);
      return [
        "M", x0, y0,
        "L", x0, ym - r,
        "A", r, r, 0, 0, 1, xa, ya,
        "A", 1, 1, 0, 0, 0, x1, yd,
        "L", x1, y1
      ];
    }
    return [
      "M", x0, y0,
      "L", x0, ym - r,
      "A", r, r, 0, 0, 1, x0 - r, ym,
      "L", x1 + 1, ym,
      "A", 1, 1, 0, 0, 0, x1, ym + 1,
      "L", x1, y1
    ];
  }
  function bendRight(x0, y0, ym, x1, y1, r) {
    if (x1 <= (x0 + r)) {
      var a = bendAngle(x1 - x0, r);
      var xa = x0 - r * (Math.cos(a) - 1);
      var ya = ym + r * (Math.sin(a) - 1);
      var yd = ya + Math.sin(a);
      return [
        "M", x0, y0,
        "L", x0, ym - r,
        "A", r, r, 0, 0, 0, xa, ya,
        "A", 1, 1, 0, 0, 1, x1, yd,
        "L", x1, y1
      ];
    }
    return [
      "M", x0, y0,
      "L", x0, ym - r,
      "A", r, r, 0, 0, 0, x0 + r, ym,
      "L", x1 - 1, ym,
      "A", 1, 1, 0, 0, 1, x1, ym + 1,
      "L", x1, y1
    ];
  }
  function connector(from, to, phase) {
    var from_xy = outgoing_xy(from, to, phase);
    var to_xy = incoming_xy(to, from, phase);
    var x0 = from_xy.x;
    var y0 = from_xy.y;
    var x1 = to_xy.x;
    var y1 = to_xy.y;
    var ym = elbow_y(from, to);
    console.assert(y0 < ym);
    if (x0 === x1) {
      console.assert(ym <= y1);
    } else {
      console.assert(ym < y1);
    }
    switch (phase) {
      case 0:
        // SVG
        var bend = from.outgoing.find(x => x.to === to).bend;
        y0 *= scale_y;
        y1 *= scale_y;
        var path;
        if (x0 < x1) {
          ym = (from.y + from.elbow + 4) * scale_y + bend;
          path = bendRight(x0, y0, ym, x1, y1, 1 + bend);
        } else if (x0 > x1) {
          ym = (from.y + from.elbow + 4) * scale_y - bend;
          path = bendLeft(x0, y0, ym, x1, y1, 1 - bend);
        } else {
          path = ["M", x0, y0, "L", x1, y1];
        }
        var years = (to.year || 2018) - from.year;
        svg = element("path", { d: path.join(" "), stroke: hsla(from, 40, 60 - Math.round(years * 0.7)), "stroke-width": 0.9, fill:"none" }) + svg;
        break;
      case 1:
        // TXT
        if (x0 < x1) {
          horizontal(x0 + 1, ym, x1 - x0 - 1);
        } else if (x0 > x1) {
          horizontal(x1 + 1, ym, x0 - x1 - 1);
        }
        break;
      case 2:
        if (x0 < x1) {
          vertical(x0, y0 + 1, ym - y0 - 1);
          vertical(x1, ym + 1, y1 - ym - 1);
        } else if (x0 > x1) {
          vertical(x0, y0 + 1, ym - y0 - 1);
          vertical(x1, ym + 1, y1 - ym - 1);
        } else {
          vertical(x0, y0 + 1, y1 - y0 - 1);
        }
        break;
      case 3:
        if (x0 < x1) {
          set(x0, ym, "╰");
          set(x1, ym, "╮");
        } else if (x0 > x1) {
          set(x0, ym, "╯");
          set(x1, ym, "╭");
        }
        break;
    }
  }
  function draw() {
    // TXT/SVG
    var from;
    for (from of data) {
      label(from);
    }
    for (var phase = 0; phase <= 3; ++phase) {
      for (from of data) {
        for (var to of from.outgoing) {
          connector(from, to.to, phase);
        }
      }
    }
    var rows = flow.length;
    var cols = Math.max(...flow.map(x => x.length));
    var message = "egg.chilliant.com";
    set(cols - message.length, rows - 1, message);
    textflow.textContent = flow.join("\n");
    // SVG
    svgflow.innerHTML = svg;
    var bbox = svgflow.getBBox();
    var w = Math.floor(bbox.width);
    var h = Math.floor(bbox.height);
    svg += link("http://egg.chilliant.com/", element("text", { x: w, y: h, "font-size": 1.5, "text-anchor": "end", fill: "grey" }, message));
    var vbox = { x: bbox.x - 1, y: bbox.y - 1, width: w + 2, height: h + 2, fill: "white" };
    svgflow.setAttribute("viewBox", [vbox.x, vbox.y, vbox.width, vbox.height].join(" "));
    svgflow.innerHTML = element("rect", vbox) + svg;
  }
  initialise();
  output();
})();
