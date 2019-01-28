/*jslint esversion:6*/
/*jslint bitwise:true*/
/*jshint loopfunc:true*/
/*eslint no-extra-parens:0*/
/*globals egg*/
if (typeof egg === "undefined") {
  egg = {};
}
egg.bnf = function(data) {
  egg.data = data;
};
egg.syntax = function(syntax, div) {
  "use strict";
  var hue = {
    // egg: hsl256(21,255,238) = hsl(h,100%,93%)
    // egg: hsl100(h,100,238)
    // hue256: 21, 57, 94,  131, 167, 204, 240
    // hue360: 30, 81, 132, 184, 235, 287, 338
    definition: 184, // azure
    rule: 30, // egg
    track: 132, // green
    terminal: 235, // blue
    token: 287 // purple
  };
  function hsl(what, lightness) {
    return "hsl(" + hue[what] + ",100%," + lightness + ")";
  }
  function stroke(what) {
    return hsl(what, "25%");
  }
  function fill(what) {
    return hsl(what, "93%");
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
  function spanRule(rule) {
    return element("span", { class: "rule" }, "&lt;" + rule + "&gt;");
  }
  function spanToken(token) {
    return element("span", { class: "token" }, "'" + token + "'");
  }
  function spanTerminal(terminal) {
    return element("span", { class: "terminal" }, "[" + terminal + "]");
  }
  function construct(src, rules, delist) {
    var constructed = Object.assign({}, src);
    for (var key of Object.keys(rules)) {
      if (rules[key] === null) {
        delete constructed[key];
      } else {
        constructed[key] = rules[key];
      }
    }
    var referenced = {};
    function ref(from, to) {
      if (!referenced[to]) {
        referenced[to] = {};
      }
      referenced[to][from] = true;
    }
    rules = Object.keys(constructed);
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
          dst.inline = false;
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
        case "alias":
        case "rule":
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
          dst.inline = false;
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
  function ascii(rulebase) {
    var rules = Object.keys(rulebase);
    function expand(rule, parentheses) {
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
        case "alias":
          return spanRule(value);
        case "terminal":
          return spanTerminal(value);
        case "token":
          return spanToken(value);
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
        case "alias":
        case "terminal":
          return value;
        case "token":
          return "'" + value + "'";
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
  function railroad(rulebase, mega, scale) {
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
      function makeRule(item) {
        return { type: "rule", item: item, end: 0.5 };
      }
      function makeDefinition(rule) {
        return { type: "definition", name: rule.name, item: expand(rule, rule.name), left: rule.left || 0.5, right: rule.right || 0.5 };
      }
      function expand(rule, inside) {
        if (typeof rule === "string") {
          if (rulebase[rule].inline === false) {
            if (mega && !mega.root) {
              if (!(rule in mega) || (mega[rule] === inside)) {
                mega[rule] = true;
                return makeDefinition(rulebase[rule]);
              }
            }
            if (mega[rule] !== null) {
              return makeBox("rule", rule);
            }
          }
          rule = rulebase[rule];
        }
        var key = Object.keys(rule)[0];
        var value = rule[key];
        var result;
        switch (key) {
          case "sequence":
            return makeSequence(value.map(x => expand(x, inside)));
          case "choice":
            return makeChoice(value.map(x => expand(x, inside)), false);
          case "zeroOrOne":
            value = expand(value, inside);
            if (value.type === "choice") {
              value.optional = true;
              return value;
            }
            return makeZeroOrOne(value);
          case "zeroOrMore":
            value = expand(value, inside);
            switch (value.type) {
              case "sequence":
              case "definition":
                return makeOneOrMore(makeZeroOrOne(value));
              case "choice":
                value.optional = true;
                return makeOneOrMore(value);
            }
            return makeZeroOrMore(value);
          case "oneOrMore":
            return makeOneOrMore(expand(value, inside));
          case "list":
            return makeList(expand(value, inside), expand(rule.separator, inside));
          case "tokens":
            return makeChoice(value.map(x => makeBox("token", x)), false);
          case "alias":
            return makeBox("terminal", rule.name);
          case "rule":
            return makeBox("rule", value);
        }
        return makeBox(key, value);
      }
      return makeRule(makeDefinition(rule));
    }
    function measure(item) {
      var i;
      switch (item.type) {
      case "stack":
      case "choice":
        var height = 0;
        if (item.optional) {
          item.width = 0;
          item.above = 0.5;
          height = 1;
        }
        for (i of item.items) {
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
        for (i of item.items) {
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
      case "definition":
        item.width = measure(item.item) + item.left + item.right;
        item.above = item.item.above + 2.25;
        item.below = item.item.below + 1;
        break;
      case "rule":
        item.width = measure(item.item) + item.end * 2;
        item.above = item.item.above;
        item.below = item.item.below - 0.25;
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
      return element("line", { x1: x0, y1: y0, x2: x1, y2: y1, stroke: stroke("track"), "stroke-width": 0.1, "stroke-linecap": "square", fill: "none"});
    }
    function circle(x, y, r) {
      return element("ellipse", { cx: x, cy: y, rx: r, ry: r, stroke: stroke("track"), "stroke-width": 0.1, fill: "none"});
    }
    function rounded(x, y, w, h, r, fill) {
      return element("rect", { x: x, y: y, width: w, height: h, fill: fill, stroke: "none", rx: r, ry: r });
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
      return element("path", { d: path.join(" "), stroke: stroke("track"), "stroke-width": 0.1, "stroke-linecap": "square", fill: "none"});
    }
    function box(x, y, w, h, text, shape) {
      var style = "italic";
      y -= h * 0.5;
      var svg;
      switch (shape) {
      case "rule":
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: fill("rule"), stroke: stroke("rule"), "stroke-width": 0.1, rx: 0.4, ry: 0.4 });
        break;
      case "token":
        style = "normal";
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: fill("token"), stroke: stroke("token"), "stroke-width": 0.1 });
        break;
      case "terminal":
        var dx = h / 3;
        var polygon = [
          x, y + h * 0.5,
          x + dx, y,
          x + w - dx, y,
          x + w, y + h * 0.5,
          x + w - dx, y + h,
          x + dx, y + h
        ];
        svg = element("polygon", { points: polygon.join(" "), fill: fill("terminal"), stroke: stroke("terminal"), "stroke-width": 0.1 });
        break;
      default:
        svg = element("rect", { x: x, y: y, width: w, height: h, fill: "white", stroke: "red", "stroke-width": 0.1 });
        break;
      }
      var fontsize = h * 0.7;
      x += w * 0.5;
      y += fontsize;
      svg += element("text", { x: x, y: y, "font-family": "monospace", "font-size": fontsize, "font-weight": "bold", "font-style": style, "text-anchor": "middle", fill: stroke(shape) }, text);
      return svg; //link("https://en.wikipedia.org/wiki/" + i.wiki, part);
    }
    function loop(x0, y0, x1, y1, r, yline) {
      var svg = "";
      if ((y1 - r) < (y0 + r + 1e-8)) {
        svg += arc(x0, y1, r, "we");
        svg += arc(x1, y0, r, "ew");
      } else {
        svg += arc(x0, y1, r, "wn");
        svg += line(x0 - r, y1 - r, x0 - r, y0 + r);
        svg += arc(x0 - r, y0 + r, r, "ne");
        svg += arc(x1, y0, r, "es");
        svg += line(x1 + r, y0 + r, x1 + r, y1 - r);
        svg += arc(x1, y1, r, "en");
      }
      if (yline !== null) {
        svg += line(x0, yline, x1, yline);
      }
      return svg;
    }
    function draw(item, x, y, w) {
      var i, svg = "";
      switch (item.type) {
      case "stack":
        y -= item.items[0].above;
        for (i of item.items) {
          y += i.above;
          svg += draw(i, x, y, w);
          y += i.below + item.between;
        }
        break;
      case "sequence":
        for (i of item.items) {
          svg += draw(i, x, y, i.width);
          x += i.width + item.between;
        }
        break;
      case "choice":
        var joint = null;
        svg += arc(x, y, 0.5, "es");
        svg += arc(x + w, y, 0.5, "ws");
        if (item.optional) {
          y += 0.5;
          joint = y;
        }
        for (i of item.items) {
          if (joint === null) {
            joint = y + 0.5;
          } else {
            y += i.above;
            svg += line(x + 0.5, joint, x + 0.5, y - 0.5) + arc(x + 0.5, y - 0.5, 0.5, "se");
            svg += arc(x + w - 1, y, 0.5, "en") + line(x + w - 0.5, joint, x + w - 0.5, y - 0.5);
            joint = y - 0.5;
            svg += line(x + 1, y, x + w - 1, y);
          }
          svg += draw(i, x + 1, y, w - 2);
          y += i.below + item.between;
        }
        break;
      case "rule":
        svg += line(x + 0.2, y, x + item.width - 0.2, y);
        svg += draw(item.item, x + item.end, y, item.width - item.end * 2);
        svg += circle(x, y, 0.2);
        svg += circle(x + item.width, y, 0.2);
        break;
      case "definition":
        var inside = hsl("definition", (mega && !mega.root) ? "100%" : "95%");
        svg += rounded(x, y - item.above + 0.75, item.name.length * 0.36 + 0.4, 1.5, 0.25, hsl("definition", "75%"));
        svg += rounded(x, y - item.above + 1.5, item.width, item.above + item.below - 1.5, 0.5, hsl("definition", "75%"));
        svg += rounded(x + 0.25, y - item.above + 1.75, item.width - 0.5, item.above + item.below - 2, 0.25, inside);
        svg += element("text", { x: x + 0.2, y: y - item.above + 1.4, "font-family": "monospace", "font-size": 0.65, "font-style": "italic", "text-anchor": "left", fill: stroke("definition") }, item.name);
        svg += line(x, y, x + item.width, y);
        svg += draw(item.item, x + item.left, y, item.width - item.left - item.right);
        break;
      case "zeroOrOne":
        i = y + item.item.above + 0.5;
        svg += line(x + 1, i, x + w - 1, i);
        svg += arc(x, y, 0.5, "es");
        svg += arc(x + 0.5, y + item.item.above, 0.5, "se");
        svg += arc(x + item.item.width + 1, i, 0.5, "en");
        svg += arc(x + item.item.width + 1.5, y + 0.5, 0.5, "ne");
        if (item.item.above !== 0.5) {
          svg += line(x + 0.5, y + 0.5, x + 0.5, y + item.item.above);
          svg += line(x + item.item.width + 1.5, y + 0.5, x + item.item.width + 1.5, y + item.item.above);
        }
        svg += draw(item.item, x + 1, i, item.item.width);
        break;
      case "zeroOrMore":
        i = y - item.item.below - 0.5;
        svg += loop(x + 0.5, i, x + w - 0.5, y, 0.5, i);
        svg += draw(item.item, x + 0.5, i, item.item.width);
        break;
      case "oneOrMore":
        i = y - item.item.above - 0.5;
        svg += loop(x + 0.5, i, x + w - 0.5, y, 0.5, i);
        svg += draw(item.item, x + 0.5, y, item.item.width);
        break;
      case "list":
        i = y - item.item.above - 0.5;
        svg += loop(x + 0.5, i, x + w - 0.5, y, 0.5, i);
        svg += draw(item.separator, x + (w - item.separator.width) / 2, i, item.separator.width);
        svg += draw(item.item, x + (w - item.item.width) / 2, y, w - 1);
        break;
      case "box":
        svg += box(x, y, item.width, 0.8, item.text, item.shape);
        break;
      default:
        console.error("Unknown railroad node: " + JSON.stringify(item));
        break;
      }
      return svg;
    }
    var rule;
    for (rule of rules) {
      if (rulebase[rule].railroad === false) {
        rulebase[rule].inline = true;
      }
    }
    var built = {
      type: "stack",
      x: 0,
      y: 0,
      between: 1,
      items: []
    };
    if (mega) {
      built.items.push(build(rulebase[mega.root || "module"]));
    } else {
      for (rule of rules) {
        if (rulebase[rule].inline === false) {
          built.items.push(build(rulebase[rule]));
        }
      }
    }
    measure(built);
    var svg = draw(built, 0.5, built.above, built.width);
    var viewbox = [0, 0, built.width + 1, built.above + built.below + 1];
    if (scale) {
      scale *= 1.2;
      svg = element("g", { transform: "scale(" + scale + ")" }, svg);
      return element("svg", { xmlns: "http://www.w3.org/2000/svg", version: "1.1", width: viewbox[2] * scale, height: viewbox[3] * scale }, svg);
    }
    return element("svg", { xmlns: "http://www.w3.org/2000/svg", version: "1.1", viewBox: viewbox.join(" ") }, svg);
  }
  div = div || document.getElementById(syntax);
  if (syntax instanceof Object) {
    div.innerHTML = "&nbsp;"; // So the line height is recalculated for a single line
    div.innerHTML = railroad(construct(syntax, {}, false), null, div.offsetHeight);
  } else {
    switch (syntax) {
      case "ascii":
        div.innerHTML = ascii(construct(egg.data.rules, egg.data.variations.full, true));
        break;
      case "bottlecaps": // http://www.bottlecaps.de/rr/ui
        div.textContent = bottlecaps(construct(egg.data.rules, egg.data.variations.full, true));
        break;
      case "railroad":
        div.innerHTML = railroad(construct(egg.data.rules, egg.data.variations.concise, false), null, div.offsetHeight);
        break;
      case "poster":
        div.innerHTML = railroad(construct(egg.data.rules, egg.data.variations.concise, false), {
          "attribute": "definition-function-parameter",
          "parameter-list": "expression-primary",
          "expression": "statement-action",
          "expression-unary": "expression-binary",
          "expression-primary": "expression-unary",
          "definition-function-parameter": "definition-function",
          "type-expression": "definition-type",
          "type-expression-primary": null,
          "statement-switch": null,
          "statement-while": null,
          "statement-do": null,
          "statement-for": null,
          "statement-foreach": null,
          "statement-break": null,
          "statement-continue": null,
          "statement-throw": null,
          "statement-try": null,
          "statement-try-finally": null,
          "statement-return": null,
          "statement-yield": null,
          "statement-call": null,
          "literal-type": "definition-type"
        });
        break;
      default:
        div.innerHTML = railroad(construct(egg.data.rules, egg.data.variations.concise, false), { root: syntax }, div.offsetHeight);
        break;
    }
  }
};
egg.code = function(element) {
  function enspan(src) {
    var dst = "";
    while (src.length) {
      var match;
      if ((match = src.match(/^\/\/.*?$/)) || (match = src.match(/^\/\/.*?\n/)) || (match = src.match(/^\/\*[\s\S]*?\*\//))) {
        dst += "<span class='code-comment'>" + src.slice(0, match[0].length) + "</span>";
      } else if ((match = src.match(/^"[^"\\]*(?:\\[\s\S][^"\\]*)*"/))) {
        dst += "<span class='code-string'>" + src.slice(0, match[0].length) + "</span>";
      } else if ((match = src.match(/^[a-zA-Z_][a-zA-Z0-9_]*/))) {
        switch (match[0].trim()) {
          case "any":
          case "bool":
          case "break":
          case "case":
          case "catch":
          case "continue":
          case "default":
          case "do":
          case "else":
          case "false":
          case "finally":
          case "float":
          case "for":
          case "if":
          case "int":
          case "null":
          case "object":
          case "return":
          case "string":
          case "switch":
          case "throw":
          case "true":
          case "try":
          case "type":
          case "var":
          case "void":
          case "while":
          case "yield":
            dst += "<span class='code-keyword'>" + src.slice(0, match[0].length) + "</span>";
            break;
          default:
            dst += "<span class='code-identifier'>" + src.slice(0, match[0].length) + "</span>";
            break;
        }
      } else {
        match = src.match(/^[\s\S][^"/a-zA-Z_]*/);
        dst += "<span>" + src.slice(0, match[0].length) + "</span>";
      }
      src = src.slice(match[0].length);
    }
    return dst;
  }
  element.innerHTML = enspan(element.textContent);
};
egg.anchor = function(text) {
  return text.toLowerCase().replace(/[^a-z0-9]+/g, " ").trim().replace(/ /g, "-");
};
egg.collapsible = function(elements) {
  for (var element of elements) {
    var eye = document.createElement("a");
    eye.href = "#";
    eye.tabIndex = -1;
    eye.onclick = function(event) {
      var style = this.parentElement.lastElementChild.style;
      style.display = (style.display !== "block") ? "block" : "none";
      event.preventDefault();
    };
    eye.innerHTML = "&#x1F441;";
    element.insertBefore(eye, element.firstChild);
  }
};
egg.toc = function(id, anchors) {
  var parent = document.getElementById(id);
  var values = [0, 0, 0, 0, 0, 0];
  var known = {};
  for (var element of document.querySelectorAll("h1,h2,h3,h4,h5,h6")) {
    element.id = egg.anchor(element.textContent);
    console.assert(!(element.id in known));
    var child = document.createElement("div");
    var level = +element.tagName.slice(1);
    values[level - 1]++;
    var section = values.slice(0, level).join(".") + "." + " " + element.innerHTML;
    child.innerHTML = "<a class='toc-" + level + "' href='#" + element.id + "'>" + section + "</a>";
    element.innerHTML = section + " <a class='link' href='#" + element.id + "'>&sect;</a>";
    parent.appendChild(child);
    while (level < values.length) {
      values[level++] = 0;
    }
    known[element.id] = section;
  }
  if (anchors) {
    for (var anchor of anchors) {
      if (!anchor.href) {
        var link = known[anchor.textContent];
        console.assert(link);
        anchor.href = "#" + anchor.textContent;
        anchor.innerHTML = "&sect;" + link;
      } else if (!anchor.hash) {
        anchor.innerHTML += "&#x1F87D;";
      }
    }
  }
};
egg.year = function(id) {
  document.getElementById(id).textContent = new Date().getFullYear();
};
egg.diagram = function(elements) {
  for (var element of elements) {
    egg.syntax(JSON.parse(element.textContent), element);
  }
};
egg.railroad = function(elements) {
  for (var element of elements) {
    egg.syntax(element.textContent, element);
  }
};
egg.prettify = function(elements) {
  for (var element of elements) {
    egg.code(element);
  }
};
