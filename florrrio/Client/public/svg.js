// Paste your SVG path here
const svgPath = `m33.13 46.498 38.765-12.595q.133-.043.273-.06.14-.016.28-.005t.276.049.26.102q.126.064.236.15.111.088.202.195t.16.23.113.256q.052.161.065.33.014.168-.013.335-.026.167-.091.323-.065.157-.164.293L49.534 69.077q-.083.113-.186.209t-.223.169-.252.122-.271.07q-.14.023-.28.017t-.277-.038q-.137-.033-.265-.092t-.241-.141q-.137-.1-.247-.228-.11-.129-.187-.28-.076-.15-.116-.315-.04-.164-.04-.333v-40.76q0-.14.028-.278t.081-.268.132-.247.178-.217.217-.177q.117-.079.247-.132t.268-.082q.138-.027.278-.027.17 0 .334.04.164.039.315.115.15.077.279.187t.228.247l23.958 32.975q.083.114.141.241.06.128.092.265.033.137.039.277t-.017.28-.07.27q-.05.133-.123.253t-.169.223-.21.186q-.136.1-.292.164-.156.065-.323.091-.167.027-.336.014-.169-.014-.33-.066L33.13 49.216q-.134-.044-.257-.112-.122-.07-.23-.16-.106-.092-.193-.202t-.151-.236-.103-.261-.049-.276.006-.28.06-.273q.052-.161.14-.305t.208-.264.264-.208.305-.14m.883 2.718-.442-1.359.442-1.359 38.765 12.596-.442 1.358-1.156.84-23.958-32.975 1.156-.84h1.429v40.76h-1.429l-1.156-.84L71.18 34.422l1.156.84.442 1.358z`;

// Use a path parser library (available in browsers and Node)
const { parseSVG, makeAbsolute } = require('svg-path-parser');

const commands = makeAbsolute(parseSVG(svgPath));

// Generate Canvas code
let jsCode = "ctx.beginPath();\n";
for (const c of commands) {
  switch (c.command) {
    case 'moveto':
      jsCode += `ctx.moveTo(${c.x}, ${c.y});\n`;
      break;
    case 'lineto':
      jsCode += `ctx.lineTo(${c.x}, ${c.y});\n`;
      break;
    case 'curveto':
      jsCode += `ctx.bezierCurveTo(${c.x1}, ${c.y1}, ${c.x2}, ${c.y2}, ${c.x}, ${c.y});\n`;
      break;
    case 'quadto':
      jsCode += `ctx.quadraticCurveTo(${c.x1}, ${c.y1}, ${c.x}, ${c.y});\n`;
      break;
    case 'closepath':
      jsCode += `ctx.closePath();\n`;
      break;
  }
}
jsCode += "ctx.stroke();";

console.log(jsCode);
