<?xml version="1.0" encoding="utf-8"?>

<!-- purpose: remove all non-ascii chars DocBook XSL places in -->
<!-- generated HTML files as the HTML -> plain ASCII text      -->
<!-- transition doesn't work that way                          -->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output 
    method="xml" 
    doctype-public="//W3C//DTD XHTML 1.0 Transitional//EN"
    doctype-system="ttp://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"
    indent="no"
    />

  <xsl:strip-space elements="*"/>

  <!-- as default, copy each node as-is -->
  <xsl:template match="/ | node() | @* | comment() | processing-instruction()">
    <xsl:copy>
      <xsl:apply-templates select="@* | node()"/>
    </xsl:copy>
  </xsl:template>

  <!-- fixup all #text parts -->
  <xsl:template match="text()">
    <xsl:call-template name="fixup">
      <xsl:with-param name="str"><xsl:value-of select="."/></xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <!-- fixup all elements with title attributes, add more if needed -->
  <xsl:template match="@title">
    <xsl:attribute name="title">
      <xsl:call-template name="fixup">
        <xsl:with-param name="str"><xsl:value-of select="."/></xsl:with-param>
      </xsl:call-template>
    </xsl:attribute>
  </xsl:template>

  <!-- fixup a single string: -->
  <!-- 1) replace all non-breaking spaces by ascii-spaces (0xA0) -->
  <!-- 2) replace all quotes by ascii quotes (0x201C and 0x201D) -->
  <!-- 3) replace "fancy" tilde with real tilde (sic!) (0x2DC)   -->
  <!-- 4) replace "horizontal ellipses" with 3 dots (0x2026)     -->
  <xsl:template name="fixup">
    <xsl:param name="str"/>
    <xsl:choose>
      <xsl:when test="contains($str,'&#x2026;')">
        <xsl:call-template name="fixup">
          <xsl:with-param name="str"><xsl:value-of select="substring-before($str,'&#x2026;')"/></xsl:with-param>
        </xsl:call-template>
        <xsl:text>...</xsl:text>
        <xsl:call-template name="fixup">
          <xsl:with-param name="str"><xsl:value-of select="substring-after($str,'&#x2026;')"/></xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="translate(translate(translate(translate($str,'&#xA0;',' '),'&#x201C;','&#x22;'),'&#x201D;','&#x22;'),'&#x2DC;','~')"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
