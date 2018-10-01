/*jslint esversion:6*/
/*jslint bitwise:true*/
/*jshint loopfunc:true*/
/*eslint no-extra-parens:0*/
/*globals egg*/
if (typeof egg === "undefined") {
  egg = {};
}
egg.bnf = function(data) {
  "use strict";
  function safe(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
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
  function spanRule(rule) {
    return element("span", { class: "rule" }, "&lt;" + rule + "&gt;");
  }
  function spanToken(token) {
    return element("span", { class: "token" }, "'" + token + "'");
  }
  function spanTerminal(terminal) {
    return element("span", { class: "terminal" }, "[" + terminal + "]");
  }
  function construct(src, variation, delist) {
    var constructed = Object.assign({}, src);
    for (var key of Object.keys(variation.rules)) {
      if (variation.rules[key] === null) {
        delete constructed[key];
      } else {
        constructed[key] = variation.rules[key];
      }
    }
    var referenced = {};
    function ref(from, to) {
      if (!referenced[to]) {
        referenced[to] = {};
      }
      referenced[to][from] = true;
    }
    var rules = Object.keys(constructed);
    function clone(name, src) {
      var dst = Object.assign({}, src);
      var key = Object.keys(src)[0];
      if (!key) {
        console.error("'" + name + "'", "is invalid object:", JSON.stringify(src));
        return null;
      }
      var value = src[key];
      switch (key) {
        case "list":
          if (typeof value !== "string") {
            console.error("'" + name + "'", key, "expected to be string");
            return null;
          }
          if (!("separator" in src)) {
            console.error("'" + name + "'", key, "is missing separator property");
            return null;
          }
          if (!delist) {
            dst.separator = clone(name, dst.separator);
            if (dst.separator === null) {
              return null;
            }
            ref(name, value);
            break;
          }
          key = "choice";
          value = [value, { sequence: [name, src.separator, value] }];
          dst = { choice: value };
          /*fallsthrough*/
        case "choice":
          dst.collapse = false;
          /*fallsthrough*/
        case "sequence":
          if (!Array.isArray(value)) {
            console.error("'" + name + "'", key, "expected to be an array");
            return null;
          }
          dst[key] = [];
          for (var i = 0; i < value.length; ++i) {
            switch (typeof value[i]) {
              case "string":
                if (!rules.includes(value[i])) {
                  console.warn("'" + name + "'", key, "has unknown rule: '" + value[i] + "'");
                  return null;
                }
                dst[key][i] = value[i];
                ref(name, value[i]);
                break;
              case "object":
                dst[key][i] = clone(name, value[i]);
                if (dst[key][i] === null) {
                  return null;
                }
                break;
              default:
                console.error("'" + name + "'", key, "has bad element " + i + ":", JSON.stringify(value[i]));
                return null;
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
                return null;
              }
              ref(name, value);
              break;
            case "object":
              dst[key] = clone(name, value);
              if (dst[key] === null) {
                return null;
              }
              break;
            default:
              console.error("'" + name + "'", key, "has bad value:", JSON.stringify(value));
              return null;
          }
          break;
        case "token":
        case "terminal":
          if (typeof value !== "string") {
            console.error("'" + name + "'", key, "has bad value:", JSON.stringify(value));
            return null;
          }
          break;
        case "tokens":
          if (!Array.isArray(value)) {
            console.error("'" + name + "'", key, "expected to be an array");
            return null;
          }
          for (var j of value) {
            if (typeof j !== "string") {
              console.error("'" + name + "'", key, "has bad element " + i + ":", JSON.stringify(j));
              return null;
            }
          }
          dst.collapse = false;
          break;
        default:
          console.error("Invalid tag for '" + name + "':", JSON.stringify(key));
          return null;
      }
      return dst;
    }
    for (var rule of rules) {
      var cloned = clone(rule, constructed[rule]);
      if (cloned === null) {
        return null;
      }
      cloned.name = rule;
      cloned.refs = referenced[rule];
      constructed[rule] = cloned;
    }
    return constructed;
  }
  function ascii(rulebase, collapsed, annotated) {
    var rules = Object.keys(rulebase);
    function collapse(rule) {
      if (collapsed && (typeof rule === "string")) {
        switch (rulebase[rule].collapse) {
          case true:
            return rulebase[rule];
          case false:
            return null;
        }
        if (rulebase[rule].refs && (Object.keys(rulebase[rule].refs).length === 1)) {
          return rulebase[rule];
        }
      }
      return null;
    }
    function expand(rule, parentheses) {
      rule = collapse(rule) || rule;
      if (typeof rule === "string") {
        return spanRule(rule);
      }
      var key = Object.keys(rule)[0];
      var value = rule[key];
      var result;
      switch (key) {
        case "sequence":
          result = value.map(x => expand(x, false)).join(" ");
          break;
        case "list":
          result = "(" + spanRule(value) + " " + expand(rule.separator) + ")* " + spanRule(value);
          break;
        case "zeroOrOne":
          return expand(value, true) + "?";
        case "zeroOrMore":
          return expand(value, true) + "*";
        case "oneOrMore":
          return expand(value, true) + "+";
        case "token":
          return spanToken(value);
        case "terminal":
          return spanTerminal(value);
        case "tokens":
          return value.map(x => spanToken(x)).join(" | ");
        default:
          console.error("Invalid ascii tag:", JSON.stringify(key));
          return "<unknown>";
      }
      if (parentheses) {
        return "(" + result + ")";
      }
      return result;
    }
    var html = "";
    for (var rule of rules) {
      if (annotated || !collapse(rule)) {
        var lines = "";
        var before = spanRule(rule) + " ::= ";
        if (rulebase[rule].choice) {
          for (var choice of rulebase[rule].choice) {
            lines += before + expand(choice, false);
            before = "\n" + " ".repeat(rule.length + 5) + "| ";
          }
        } else if (rulebase[rule].list) {
          lines += before + expand(rulebase[rule].list);
          lines += "\n" + " ".repeat(rule.length + 5) + "| " + spanRule(rule) + " " + expand(rulebase[rule].separator) + " " + expand(rulebase[rule].list);
        } else {
          lines = before + expand(rulebase[rule], false);
        }
        html += element("pre", {}, lines);
        if (annotated) {
          if (!rulebase[rule].refs) {
            html += element("div", { class: "unused" }, "Unused");
          } else if (collapse(rule)) {
            html += element("div", { class: "collapsed" }, "Collapsed into " + Object.keys(rulebase[rule].refs).join(", "));
          } else {
            html += element("div", { class: "used" }, "Used by " + Object.keys(rulebase[rule].refs).join(", "));
          }
        }
      }
    }
    return html;
  }
  function bottlecaps(rulebase) {
    var rules = Object.keys(rulebase);
    function expand(rule, parentheses) {
      if (typeof rule === "string") {
        return rule;
      }
      var key = Object.keys(rule)[0];
      var value = rule[key];
      var result;
      switch (key) {
        case "sequence":
          result = value.map(x => expand(x, false)).join(" ");
          break;
        case "zeroOrOne":
          return expand(value, true) + "?";
        case "zeroOrMore":
          return expand(value, true) + "*";
        case "oneOrMore":
          return expand(value, true) + "+";
        case "token":
          return "'" + value + "'";
        case "terminal":
          return value;
        case "tokens":
          return value.map(x => "'" + x + "'").join(" | ");
        default:
          console.error("Invalid bottlecaps tag:", JSON.stringify(key));
          return "[unknown]";
      }
      if (parentheses) {
        return "(" + result + ")";
      }
      return result;
    }
    var text = [];
    for (var rule of rules) {
      var lines = "";
      var before = rule + " ::= ";
      if (rulebase[rule].choice) {
        for (var choice of rulebase[rule].choice) {
          lines += before + expand(choice, false);
          before = "\n" + " ".repeat(rule.length + 3) + "| ";
        }
      } else {
        lines = before + expand(rulebase[rule], false);
      }
      text.push(lines);
    }
    return text.join("\n\n");
  }
  function railroad(rulebase) {
    var rules = Object.keys(rulebase);
    function build(rule) {
      function makeBox(shape, name) {
        return { type: "box", shape: shape, width: name.length * 0.3 + 1, text: name };
      }
      function makeSequence(items) {
        return { type: "sequence", between: 0.5, items: items };
      }
      function makeChoice(items) {
        return { type: "choice", between: 0.2, items: items };
      }
      function makeZeroOrOne(item) {
        return { type: "zeroOrOne", item: item };
      }
      function makeZeroOrMore(item) {
        if (item.type === "sequence") {
          item.items.reverse();
        }
        return { type: "zeroOrMore", item: item };
      }
      function makeOneOrMore(item) {
        return { type: "oneOrMore", item: item };
      }
      function makeList(item, separator) {
        return { type: "list", item: item, separator: separator };
      }
      function makeRule(name, item) {
        switch (item.type) {
        case "choice":
          return { type: "rule", name: name, item: item, end: 0.1 };
        case "sequence":
        case "box":
          return { type: "rule", name: name, item: item, end: 1.1 };
        }
        return { type: "rule", name: name, item: item, end: 0.6 };
      }
      function expand(rule) {
        if (typeof rule === "string") {
          if (rulebase[rule].collapse === false) {
            return makeBox("rule", rule);
          }
          rule = rulebase[rule];
        }
        var key = Object.keys(rule)[0];
        var value = rule[key];
        var result;
        switch (key) {
          case "sequence":
            return makeSequence(value.map(expand));
          case "choice":
            return makeChoice(value.map(expand));
          case "zeroOrOne":
            return makeZeroOrOne(expand(value));
          case "zeroOrMore":
            return makeZeroOrMore(expand(value));
          case "oneOrMore":
            return makeOneOrMore(expand(value));
          case "list":
            return makeList(expand(value), expand(rule.separator));
          case "tokens":
            return makeChoice(value.map(x => makeBox("token", x)));
        }
        return makeBox(key, value);
      }
      return makeRule(rule.name, expand(rule));
    }
    function measure(item) {
      switch (item.type) {
      case "stack":
      case "choice":
        var height = 0;
        for (var i of item.items) {
          if (height === 0) {
            item.width = measure(i);
            item.above = i.above;
            height = i.below + i.above;
          } else {
            item.width = Math.max(item.width, measure(i));
            height += i.below + i.above + item.between;
          }
        }
        item.below = height - item.above;
        if (item.type === "choice") {
          item.width += 2;
        }
        break;
      case "sequence":
        var width = 0;
        for (var i of item.items) {
          if (width === 0) {
            width = measure(i);
            item.above = i.above;
            item.below = i.below;
          } else {
            width += item.between + measure(i);
            item.above = Math.max(item.above, i.above);
            item.below = Math.max(item.below, i.below);
          }
        }
        item.width = width;
        break;
      case "zeroOrOne":
        item.width = measure(item.item) + 2;
        item.above = 0.5;
        item.below = item.item.above + item.item.below + 0.5;
        break;
      case "zeroOrMore":
        item.width = measure(item.item) + 1;
        item.above = item.item.above + item.item.below + 0.5;
        item.below = 0.5;
        break;
      case "oneOrMore":
        item.width = measure(item.item) + 1;
        item.above = item.item.above + 0.75;
        item.below = item.item.below;
        break;
      case "list":
        item.width = Math.max(measure(item.item), measure(item.separator)) + 1;
        item.above = item.separator.above + item.separator.below + item.item.above;
        item.below = item.item.below;
        break;
      case "rule":
        item.width = measure(item.item) + item.end * 2;
        item.above = item.item.above + 1;
        item.below = item.item.below;
        break;
      default:
        item.width = item.width || 0;
        item.above = item.above || 0.5;
        item.below = item.below || 0.5;
        break;
      }
      return item.width;
    }
    function line(x0, y0, x1, y1) {
      var path = ["M", x0, y0, "L", x1, y1];
      return element("path", { d: path.join(" "), stroke: "green", "stroke-width": 0.2, fill: "none"});
    }
    function circle(x, y, r) {
      return element("ellipse", { cx: x, cy: y, rx: r, ry: r, fill: "green"});
    }
    function arc(x, y, r, d) {
      var path = ["M", x, y, "A", r, r, 0, 0];
      switch (d) {
      case "wn":
        path.push(1, x - r, y - r);
        break;
      case "we":
        path.push(1, x, y - r * 2);
        break;
      case "ws":
        path.push(0, x - r, y + r);
        break;
      case "en":
        path.push(0, x + r, y - r);
        break;
      case "ew":
        path.push(1, x, y + r * 2);
        break;
      case "es":
        path.push(1, x + r, y + r);
        break;
      case "ne":
        path.push(1, x + r, y - r);
        break;
      case "se":
        path.push(0, x + r, y + r);
        break;
      default:
        console.error("Unknown railroad node arc direction: " + d);
        break;
      }
      return element("path", { d: path.join(" "), stroke: "green", "stroke-width": 0.2, fill: "none"});
    }
    function box(x, y, w, h, text, shape) {
      y -= h * 0.5;
      var svg;
      switch (shape) {
      case "rule":
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: "#FED", stroke: "blue", "stroke-width": 0.1, rx: 0.4, ry: 0.4 });
        break;
      case "token":
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: "#FED", stroke: "blue", "stroke-width": 0.1 });
        break;
      case "terminal":
        var dx = h / 3;
        var path = [
          "M", x, y + h * 0.5,
          "L", x + dx, y,
          "L", x + w - dx, y,
          "L", x + w, y + h * 0.5,
          "L", x + w - dx, y + h,
          "L", x + dx, y + h,
          "L", x, y + h * 0.5
        ];
        svg = element("path", { d: path.join(" "), fill: "#FED", stroke: "lime", "stroke-width": 0.1 });
        break;
      default:
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: "#FED", stroke: "red", "stroke-width": 0.1 });
        break;
      }
      var fontsize = h * 0.7;
      x += w * 0.5;
      y += fontsize;
      svg += element("text", { x: x, y: y, "font-family": "monospace", "font-size": fontsize, "font-weight": "bold", "text-anchor": "middle", fill: "red" }, text);
      return svg; //link("https://en.wikipedia.org/wiki/" + i.wiki, part);
    }
    function draw(item, x, y, w, thru) {
      var svg = "";
      if (item.width) {
        svg += element("rect", { x: x, y: y - item.above, width: w, height: item.above + item.below, stroke: "lime", "stroke-width": 0.02, fill: "none"});
      }
      switch (item.type) {
      case "stack":
        y -= item.items[0].above;
        for (var i of item.items) {
          y += i.above;
          svg += draw(i, x, y, w, true);
          y += i.below + item.between;
        }
        break;
      case "sequence":
        if (thru) {
          svg += line(x, y, x + w, y);
        }
        for (var i of item.items) {
          svg += draw(i, x, y, i.width, false);
          x += i.width + item.between;
        }
        break;
      case "choice":
        if (thru) {
          svg += line(x, y, x + w, y);
        }
        var joint = null;
        for (var i of item.items) {
          if (joint === null) {
            if (item.items.length !== 1) {
              svg += arc(x, y, 0.5, "es");
              svg += arc(x + w, y, 0.5, "ws");
            }
            joint = y + 0.5;
          } else {
            y += i.above;
            svg += line(x + 0.5, joint, x + 0.5, y - 0.5) + arc(x + 0.5, y - 0.5, 0.5, "se");
            svg += arc(x + w - 1, y, 0.5, "en") + line(x + w - 0.5, joint, x + w - 0.5, y - 0.5);
            joint = y - 0.5;
          }
          svg += draw(i, x + 1, y, w - 2, true);
          y += i.below + item.between;
        }
        break;
      case "rule":
        svg += element("text", { x: 0, y: y - item.above + 0.7, "font-family": "monospace", "font-size": 0.7, "font-weight": "bold", "text-anchor": "left", fill: "green" }, item.name);
        svg += line(x, y, x + item.width, y);
        svg += circle(x, y, 0.2);
        svg += circle(x + item.width, y, 0.2);
        svg += draw(item.item, x + item.end, y, item.width - item.end * 2, false);
        break;
      case "zeroOrOne":
        svg += arc(x, y, 0.5, "es");
        svg += arc(x + 0.5, y + item.item.above, 0.5, "se");
        svg += arc(x + item.item.width + 1, y + item.item.above + 0.5, 0.5, "en");
        svg += arc(x + item.item.width + 1.5, y + 0.5, 0.5, "ne");
        if (item.item.above !== 0.5) {
          svg += line(x + 0.5, y + 0.5, x + 0.5, y + item.item.above);
          svg += line(x + item.item.width + 1.5, y + 0.5, x + item.item.width + 1.5, y + item.item.above);
        }
        svg += draw(item.item, x + 1, y + item.item.above + 0.5, item.item.width, true);
        break;
      case "zeroOrMore":
        svg += arc(x + 0.5, y, 0.5, "we");
        svg += arc(x + item.item.width + 0.5, y - 1, 0.5, "ew");
        svg += draw(item.item, x + 0.5, y - item.below - 0.5, item.item.width, true);
        break;
      case "oneOrMore":
        var yy = y - item.item.above - 0.5;
        if (item.item.above === 0.5) {
          svg += arc(x + 0.5, y, 0.5, "we");
          svg += arc(x + w - 0.5, y - 1, 0.5, "ew");
        } else {
          svg += arc(x + 0.5, y, 0.5, "wn");
          svg += line(x, y - 0.5, x, yy + 0.5);
          svg += arc(x, yy + 0.5, 0.5, "ne");
          svg += arc(x + w - 0.5, yy, 0.5, "es");
          svg += line(x + w, yy + 0.5, x + w, y - 0.5);
          svg += arc(x + w - 0.5, y, 0.5, "en");
        }
        svg += line(x + 0.5, yy, x + item.item.width + 0.5, yy);
        svg += draw(item.item, x + 0.5, y, item.item.width, true);
        break;
      case "list":
        if (thru) {
          svg += line(x, y, x + w, y);
        }
        svg += arc(x + 0.5, y, 0.5, "we");
        svg += line(x + 0.5, y - 1, x + w - 0.5, y - 1);
        svg += arc(x + w - 0.5, y - 1, 0.5, "ew");
        svg += draw(item.separator, x + (w - item.separator.width) / 2, y - 1, item.separator.width, false);
        svg += draw(item.item, x + (w - item.item.width) / 2, y, w - 1, false);
        break;
      case "box":
        if (thru) {
          svg += line(x, y, x + w, y);
        }
        svg += box(x, y, item.width, 0.8, item.text, item.shape);
        break;
      case "line":
        var path = ["M", item.x0 + x, item.y0 + y, "L", item.x1 + x, item.y1 + y];
        svg += element("path", { d: path.join(" "), stroke: "green", "stroke-width": 0.2, fill: "none"});
        break;
      case "arc":
        svg += arc(item.x + x, item.y + y, 0.5, item.dir);
        break;
      default:
        console.error("Unknown railroad node: " + JSON.stringify(item));
        break;
      }
      return svg;
    }
    for (var rule of rules) {
      if (rulebase[rule].railroad === false) {
        rulebase[rule].collapse = true;
      }
    }
    var built = {
      type: "stack",
      x: 0,
      y: 0,
      between: 1,
      items: []
    };
    for (var rule of rules) {
      if (rulebase[rule].collapse === false) {
        built.items.push(build(rulebase[rule]));
      }
    }
    measure(built);
    var svg = draw(built, 0.2, built.above, built.width);
    return element("g", { transform: "scale(25)" }, svg);
  }
  var variation, rulebase;
  // ASCII
  variation = data.variations.concise;
  rulebase = construct(data.rules, variation, true);
  if (rulebase) {
    document.getElementById("ascii").innerHTML = ascii(rulebase, false, false);
  }
  // http://www.bottlecaps.de/rr/ui
  variation = data.variations.full;
  rulebase = construct(data.rules, variation, true);
  if (rulebase) {
    document.getElementById("bottlecaps").textContent = bottlecaps(rulebase);
  }
  // Railroad
  variation = data.variations.concise;
  rulebase = construct(data.rules, variation, false);
  if (rulebase) {
    document.getElementById("railroad").innerHTML = railroad(rulebase);
  }
};
