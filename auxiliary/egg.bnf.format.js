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
            dst.collapse = true;
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
          console.error("Invalid tag:", JSON.stringify(key));
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
          console.error("Invalid tag:", JSON.stringify(key));
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
      function makeBox(x, y, w, text) {
        return { type: "box", x: x, y: y, w: w, text: text };
      }
      function makeRule(name) {
        var width = name.length * 0.3 + 1;
        return { type: "rule", width: width, items: [makeBox(0, 0, width, name)] };
      }
      function makeSequence(items) {
        return { type: "sequence", between: 0.5, items: items };
      }
      function makeZeroOrMore(item) {
        return { type: "zeroOrMore", item: item };
      }
      function makeOneOrMore(item) {
        if (item.type === "sequence") {
          item.items.reverse();
        }
        return { type: "oneOrMore", item: item };
      }
      function makeApex(item) {
        return { type: "apex", item: item };
      }
      function expand(rule) {
        if (typeof rule === "string") {
          return makeRule(rule);
        }
        var key = Object.keys(rule)[0];
        var value = rule[key];
        var result;
        switch (key) {
          case "sequence":
            return makeSequence(value.map(x => expand(x)));
          case "zeroOrOne":
            return makeZeroOrOne(expand(value));
          case "zeroOrMore":
            return makeZeroOrMore(expand(value));
          case "oneOrMore":
            return makeOneOrMore(expand(value));
          case "token":
            return makeRule(value);
          case "terminal":
            return makeRule(value);
          case "tokens":
            return value.map(x => "'" + x + "'").join(" | ");
          default:
        }
        console.error("Invalid tag:", JSON.stringify(key));
        return makeRule("[unknown]");
      }
      if (rule === "module") {
        return makeApex(expand(rulebase[rule])); //makeApex(makeOneOrMore(makeSequence([makeZeroOrMore(makeRule("attribute")), makeRule("statement")])));
      }
      return makeApex(makeRule(rule));
    }
    function measure(item) {
      switch (item.type) {
      case "stack":
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
      case "zeroOrMore":
        item.width = measure(item.item) + 1;
        item.above = item.item.above + item.item.below + 0.5;
        item.below = 0.5;
        break;
      case "oneOrMore":
        item.width = measure(item.item) + 1;
        item.above = item.item.above + item.item.below + 0.5;
        item.below = 0.5;
        break;
      case "apex":
        item.width = measure(item.item) + 2;
        item.above = item.item.above;
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
      case "se":
        path.push(0, x + r, y + r);
        break;
      default:
        console.error("Unknown railroad node arc direction: " + d);
        break;
      }
      return element("path", { d: path.join(" "), stroke: "green", "stroke-width": 0.2, fill: "none"});
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
      case "apex":
        svg += line(x + 0.5, y, x + item.width - 0.5, y);
        svg += circle(x + 0.5, y, 0.2);
        svg += circle(x + item.width - 0.5, y, 0.2);
        svg += draw(item.item, x + 1, y, item.width - 2, false);
        break;
      case "zeroOrMore":
        svg += arc(x + 0.5, y, 0.5, "we");
        svg += arc(x + item.item.width + 0.5, y - 1, 0.5, "ew");
        svg += draw(item.item, x + 0.5, y - item.below - 0.5, item.item.width, true);
        break;
      case "oneOrMore":
        svg += arc(x + 0.5, y, 0.5, "we");
        svg += arc(x + item.item.width + 0.5, y - 1, 0.5, "ew");
        svg += draw(item.item, x + 0.5, y - item.below - 0.5, item.item.width, true);
        break;
      case "rule":
        if (thru) {
          svg += line(x, y, x + w, y);
        }
        for (var i of item.items) {
          svg += draw(i, x, y, w, false);
        }
        break;
      case "line":
        var path = ["M", item.x0 + x, item.y0 + y, "L", item.x1 + x, item.y1 + y];
        svg += element("path", { d: path.join(" "), stroke: "green", "stroke-width": 0.2, fill: "none"});
        break;
      case "arc":
        svg += arc(item.x + x, item.y + y, 0.5, item.dir);
        break;
      case "box":
        var ew = item.w;
        var eh = 0.8;
        var ex = item.x + x;
        var ey = item.y + y - eh * 0.5;
        var part = element("rect", { x: ex, y: ey, width: ew, height: eh, fill: "#FED", stroke: "blue", "stroke-width": 0.1, rx: 0.4, ry: 0.4 });
        ex += item.w * 0.5;
        ey += eh * 0.7;
        part += element("text", { x: ex, y: ey, "font-family": "monospace", "font-size": eh * 0.7, "font-weight": "bold", "text-anchor": "middle", fill: "red" }, item.text);
        svg += part; //link("https://en.wikipedia.org/wiki/" + i.wiki, part);
        break;
      default:
        console.error("Unknown railroad node: " + JSON.stringify(item));
        break;
      }
      return svg;
    }
    var built = {
      type: "stack",
      x: 0,
      y: 0,
      between: 1,
      items: []
    };
    for (var rule of rules) {
      built.items.push(build(rule));
    }
    measure(built);
    var svg = draw(built, 0, built.above, built.width);
    svg = element("g", { transform: "scale(25)" }, svg);
    document.getElementById("railroad").innerHTML = svg;
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
    document.getElementById("bottlecaps").textContent = railroad(rulebase);
  }
};
