/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package com.sun.scenario.effect.compiler.backend.hw;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import static java.util.Map.entry;

import com.sun.scenario.effect.compiler.JSLParser;
import com.sun.scenario.effect.compiler.model.CoreSymbols;
import com.sun.scenario.effect.compiler.model.Function;
import com.sun.scenario.effect.compiler.model.Param;
import com.sun.scenario.effect.compiler.model.Precision;
import com.sun.scenario.effect.compiler.model.Variable;
import com.sun.scenario.effect.compiler.model.Qualifier;
import com.sun.scenario.effect.compiler.model.Type;
import com.sun.scenario.effect.compiler.tree.DiscardStmt;
import com.sun.scenario.effect.compiler.tree.Expr;
import com.sun.scenario.effect.compiler.tree.JSLVisitor;
import com.sun.scenario.effect.compiler.tree.VarDecl;
import com.sun.scenario.effect.compiler.tree.CallExpr;
import com.sun.scenario.effect.compiler.tree.FuncDef;

public class MSLBackend extends SLBackend {

    private static String headerFilesDir = null;
    private static StringBuilder fragmentShaderHeader;
    private static final String FRAGMENT_SHADER_HEADER_FILE_NAME = "FragmentShaderCommon.h";
    private static final StringBuilder objCHeader = new StringBuilder();;
    private static String objCHeaderFileName;
    private static final String PRISM_SHADER_HEADER_FILE_NAME = "PrismShaderCommon.h";
    private static final String DECORA_SHADER_HEADER_FILE_NAME = "DecoraShaderCommon.h";
    private static final List<String> shaderFunctionNameList = new ArrayList<>();

    private String shaderFunctionName;
    private String textureSamplerName;
    private String sampleTexFuncName;
    private String uniformStructName;
    private List<String> uniformNames;
    private String uniformIDsEnumName;
    private String uniformIDs;
    private int uniformIDCount;
    private String uniformsForShaderFile; // visitVarDecl() accumulates all the Uniforms in this string.
    private String uniformsForObjCFiles;
    private boolean isPrismShader;

    private List<String> helperFunctions;
    private static final String MTL_HEADERS_DIR = "/mtl-headers/";
    private static final String MAIN = "void main() {";

    private static final Map<String, String> QUAL_MAP = Map.of(
        "param", "constant");

    private static final Map<String, String> TYPE_MAP = Map.of(
        "sampler",  "texture2d<float>",
        "lsampler", "texture2d<float>",
        "fsampler", "texture2d<float>");

    private static final Map<String, String> VAR_MAP = Map.ofEntries(
        entry("pos0",                     "in.texCoord0"),
        entry("pos1",                     "in.texCoord1"),
        entry("pixcoord",                 "in.pixCoord"),
        entry("color",                    "outFragColor"),
        entry("jsl_vertexColor",          "in.fragColor"),
        // The uniform variables are combined into a struct. These structs are generated while
        // parsing the jsl shader files and are added to the header files in mtl-headers directory.
        // Each fragment function receives a pointer variable named uniforms(to respective struct of Uniforms)
        // hence all the following uniform variable names must be replaced by uniforms.var_name.
        entry("weights",                  "uniforms.weights"),
        entry("kvals",                    "uniforms.kvals"),
        entry("opacity",                  "uniforms.opacity"),
        entry("offset",                   "uniforms.offset"),
        entry("shadowColor",              "uniforms.shadowColor"),
        entry("surfaceScale",             "uniforms.surfaceScale"),
        entry("level",                    "uniforms.level"),
        entry("sampletx",                 "uniforms.sampletx"),
        entry("wrap",                     "uniforms.wrap"),
        entry("imagetx",                  "uniforms.imagetx"),
        entry("contrast",                 "uniforms.contrast"),
        entry("hue",                      "uniforms.hue"),
        entry("saturation",               "uniforms.saturation"),
        entry("brightness",               "uniforms.brightness"),
        entry("tx0",                      "uniforms.tx0"),
        entry("tx1",                      "uniforms.tx1"),
        entry("tx2",                      "uniforms.tx2"),
        entry("threshold",                "uniforms.threshold"),
        entry("lightPosition",            "uniforms.lightPosition"),
        entry("lightColor",               "uniforms.lightColor"),
        entry("diffuseConstant",          "uniforms.diffuseConstant"),
        entry("specularConstant",         "uniforms.specularConstant"),
        entry("specularExponent",         "uniforms.specularExponent"),
        entry("lightSpecularExponent",    "uniforms.lightSpecularExponent"),
        entry("normalizedLightPosition",  "uniforms.normalizedLightPosition"),
        entry("normalizedLightDirection", "uniforms.normalizedLightDirection"),
        entry("fractions",                "uniforms.fractions"),
        entry("oinvarcradii",             "uniforms.oinvarcradii"),
        entry("iinvarcradii",             "uniforms.iinvarcradii"),
        entry("precalc",                  "uniforms.precalc"),
        entry("m0",                       "uniforms.m0"),
        entry("m1",                       "uniforms.m1"),
        entry("perspVec",                 "uniforms.perspVec"),
        entry("gradParams",               "uniforms.gradParams"),
        entry("idim",                     "uniforms.idim"),
        entry("gamma",                    "uniforms.gamma"),
        entry("xParams",                  "uniforms.xParams"),
        entry("yParams",                  "uniforms.yParams"),
        entry("lumaAlphaScale",           "uniforms.lumaAlphaScale"),
        entry("cbCrScale",                "uniforms.cbCrScale"),
        entry("innerOffset",              "uniforms.innerOffset"),
        entry("content",                  "uniforms.content"),
        entry("img",                      "uniforms.img"),
        entry("botImg",                   "uniforms.botImg"),
        entry("topImg",                   "uniforms.topImg"),
        entry("bumpImg",                  "uniforms.bumpImg"),
        entry("origImg",                  "uniforms.origImg"),
        entry("baseImg",                  "uniforms.baseImg"),
        entry("mapImg",                   "uniforms.mapImg"),
        entry("colors",                   "uniforms.colors"),
        entry("maskInput",                "uniforms.maskInput"),
        entry("glyphColor",               "uniforms.glyphColor"),
        entry("dstColor",                 "uniforms.dstColor"),
        entry("maskTex",                  "uniforms.maskTex"),
        entry("imageTex",                 "uniforms.imageTex"),
        entry("inputTex",                 "uniforms.inputTex"),
        entry("alphaTex",                 "uniforms.alphaTex"),
        entry("cbTex",                    "uniforms.cbTex"),
        entry("crTex",                    "uniforms.crTex"),
        entry("lumaTex",                  "uniforms.lumaTex"),
        entry("inputTex0",                "uniforms.inputTex0"),
        entry("inputTex1",                "uniforms.inputTex1")
    );

    private static final Map<String, String> FUNC_MAP = Map.of(
        "sample",  "sampleTex",
        "ddx",     "dfdx",
        "ddy",     "dfdy",
        "intcast", "int");

    // Set of functions apart from CoreSymbols.getFunctions(), that are used by our fragment shaders.
    private static final Set<String> libraryFunctionsUsedInShader = Set.of(
        "min", "max", "mix", "pow", "normalize", "abs", "fract",
        "dot", "clamp", "sqrt", "ceil", "floor", "sign", "sampleTex");

    public MSLBackend(JSLParser parser, JSLVisitor visitor) {
        super(parser, visitor);
    }

    @Override
    protected String getQualifier(Qualifier q) {
        String qual = QUAL_MAP.get(q.toString());
        return (qual != null) ? qual : q.toString();
    }

    @Override
    protected String getType(Type t) {
        String type = TYPE_MAP.get(t.toString());
        return (type != null) ? type : t.toString();
    }

    @Override
    protected String getVar(String v) {
        String s = VAR_MAP.get(v);
        return (s != null) ? s : v;
    }

    @Override
    protected String getFuncName(String f) {
        String s = FUNC_MAP.get(f);
        return (s != null) ? s : f;
    }

    @Override
    protected String getPrecision(Precision p) {
        return p.name();
    }

    @Override
    public void visitCallExpr(CallExpr e) {
        output(getFuncName(e.getFunction().getName()) + "(");
        boolean first = true;
        for (Expr param : e.getParams()) {
            if (first) {
                // For every user defined function, pass a reference to the uniforms struct
                // as first parameter.
                if (!CoreSymbols.getFunctions().contains(getFuncName(e.getFunction().getName())) &&
                        !libraryFunctionsUsedInShader.contains(getFuncName(e.getFunction().getName()))) {
                    output("textureSampler, uniforms, ");
                }
                first = false;
            } else {
                output(", ");
            }
            scan(param);
        }
        output(")");
    }

    @Override
    public void visitFuncDef(FuncDef d) {
        Function func = d.getFunction();
        helperFunctions.add(func.getName());
        output(getType(func.getReturnType()) + " " + func.getName() + "(");
        boolean first = true;
        for (Param param : func.getParams()) {
            if (first) {
                // Add "constant Uniforms& uniforms" as the first parameter to all user defined functions.
                if (!CoreSymbols.getFunctions().contains(getFuncName(d.getFunction().getName())) &&
                        !libraryFunctionsUsedInShader.contains(getFuncName(d.getFunction().getName()))) {
                    output("sampler textureSampler, device " + uniformStructName + "& uniforms, ");
                }
                first = false;
            } else {
                output(", ");
            }
            output(getType(param.getType()) + " " + param.getName());
        }
        output(") ");
        scan(d.getStmt());
    }

    @Override
    public void visitVarDecl(VarDecl d) {
        Variable var = d.getVariable();
        Qualifier qual = var.getQualifier();
        if (qual == Qualifier.CONST) { // example. const int i = 10;
            // const variables are converted into macro.
            // reason: In MSL, only the program scoped variables can be declared as constant(address space).
            // Function scope variables cannot be declared as constant.
            // In our shaders, there is one function scope variable 'const float third'
            // which causes a compilation error if all const are replaced with constant.
            // So alternate approach is to use macros for all const variables.
            output("#define " + var.getName());
            output(" (");
            scan(d.getInit());
            output(")\n");
        } else if (qual == Qualifier.PARAM) { // These are uniform variables.
            // In MSL, uniform variables can be declared by using function_constant attribute.
            // function_constant variables can only be scalar or vector type.
            // User defined type or array of scalar or vector cannot be declared as function_constants.
            // So we combine all uniform variables into a struct named Uniforms.
            String aUniform = "";
            Precision precision = var.getPrecision();
            if (precision != null) {
                String precisionStr = getPrecision(precision);
                if (precisionStr != null) {
                    aUniform += precisionStr + " ";
                }
            }
            uniformNames.add(var.getName());
            aUniform += getType(var.getType()) + " " + var.getName();
            if (var.isArray()) {
                aUniform += "[" + var.getArraySize() + "]";
            }

            if (!uniformIDs.contains(var.getName())) {
                uniformIDs += "    " + shaderFunctionName + "_" + var.getName() + "_ID = " + uniformIDCount + ",\n";
                if (var.isArray()) {
                    uniformIDCount += var.getArraySize();
                } else {
                    uniformIDCount++;
                }
            }
            if (!uniformsForShaderFile.contains(var.getName())) {
                uniformsForShaderFile += "    " + aUniform + ";\n";
            }
            if (!uniformsForObjCFiles.contains(var.getName())) {
                uniformsForObjCFiles += "    " + aUniform + ";\n";
            }
        } else {
            super.visitVarDecl(d);
        }
    }

    @Override
    public void visitDiscardStmt(DiscardStmt s) {
        output(" discard_fragment();\n");
    }

    private void updateCommonHeaders() {
        String shaderType = isPrismShader ? "PRISM" : "DECORA";

        if (fragmentShaderHeader == null) {
            fragmentShaderHeader = new StringBuilder();
            fragmentShaderHeader.append("#ifndef FRAGMENT_COMMON_H\n");
            fragmentShaderHeader.append("#define FRAGMENT_COMMON_H\n\n");
            fragmentShaderHeader.append("#include <simd/simd.h>\n");
            fragmentShaderHeader.append("#include <metal_stdlib>\n\n");
            fragmentShaderHeader.append("using namespace metal;\n\n");

            fragmentShaderHeader.append("struct VS_OUTPUT {\n");
            // TODO: MTL: Avoid passing position to fragment function if can be.
            // position is not needed in any of our fragment shaders, so we should remove it.
            // This should be done carefully. We should verify that all shaders work as expected.

            fragmentShaderHeader.append("    float4 position [[ position ]];\n");
            fragmentShaderHeader.append("    float4 fragColor;\n");
            fragmentShaderHeader.append("    float2 texCoord0;\n");
            fragmentShaderHeader.append("    float2 texCoord1;\n");
            // if (isPixcoordReferenced) {
            fragmentShaderHeader.append("    float2 pixCoord;\n");
            // }
            fragmentShaderHeader.append("};\n\n");

            try {
                FileWriter fragmentShaderHeaderFile = new FileWriter(headerFilesDir + FRAGMENT_SHADER_HEADER_FILE_NAME);
                fragmentShaderHeaderFile.write(fragmentShaderHeader.toString());
                fragmentShaderHeaderFile.write("#endif\n");
                fragmentShaderHeaderFile.close();
            } catch (IOException e) {
                System.err.println("IOException occurred while creating " + FRAGMENT_SHADER_HEADER_FILE_NAME +
                    ": " + e.getMessage());
                e.printStackTrace();
            }
        }

        try {
            FileWriter objCHeaderFile = new FileWriter(headerFilesDir + objCHeaderFileName);

            uniformsForObjCFiles = uniformsForObjCFiles.replace("texture2d<float>", "id<MTLTexture>");
            uniformsForObjCFiles = uniformsForObjCFiles.replace(" float2", " vector_float2");
            uniformsForObjCFiles = uniformsForObjCFiles.replace(" float3", " vector_float3");
            uniformsForObjCFiles = uniformsForObjCFiles.replace(" float4", " vector_float4");

            if (objCHeader.length() == 0) {
                objCHeader.append("#ifndef " + shaderType + "_SHADER_COMMON_H\n" +
                                "#define " + shaderType + "_SHADER_COMMON_H\n\n" +
                                "#import <Metal/Metal.h>\n" +
                                "#import <simd/simd.h>\n\n" +
                                "typedef struct " + shaderType + "_VS_INPUT {\n" +
                                "    vector_float2 position;\n" +
                                "    vector_float4 color;\n" +
                                "    vector_float2 texCoord0;\n" +
                                "    vector_float2 texCoord1;\n" +
                                "    vector_float2 pixCoord;\n" +
                                "} " + shaderType + "_VS_INPUT;" +
                                "\n\n");
            }

            if (uniformIDs != "") {
                objCHeader.append("typedef enum " + uniformIDsEnumName +
                    " {\n" + uniformIDs + "\n} " + shaderFunctionName + "ArgumentBufferID;\n\n");

                objCHeader.append("typedef struct " + uniformStructName + " {\n"
                    + uniformsForObjCFiles + "} " + uniformStructName + ";\n\n");

                objCHeader.append("NSDictionary* get" + shaderFunctionName + "_Uniform_VarID_Dict() {\n");
                objCHeader.append("    id ids[] = {\n");
                for (String aUniformName : uniformNames) {
                    objCHeader.append("        [NSNumber numberWithInt:" + shaderFunctionName + "_" +
                        aUniformName + "_ID" + "],\n");
                }
                objCHeader.append("    };\n\n");
                objCHeader.append("    NSUInteger count = sizeof(ids) / sizeof(id);\n");
                objCHeader.append("    NSArray *idArray = [NSArray arrayWithObjects:ids count:count];\n");

                objCHeader.append("    id uniforms[] = {\n");
                for (String aUniformName : uniformNames) {
                    objCHeader.append("        @\"" + aUniformName + "\",\n");
                }
                objCHeader.append("    };\n\n");
                objCHeader.append("    NSArray *uniformArray = [NSArray arrayWithObjects:uniforms count:count];\n");
                objCHeader.append("    return [NSDictionary dictionaryWithObjects:idArray forKeys:uniformArray];\n");
                objCHeader.append("}\n\n\n");
            } else {
                objCHeader.append("NSDictionary* get" + shaderFunctionName + "_Uniform_VarID_Dict() {\n");
                objCHeader.append("    return nil;\n");
                objCHeader.append("}\n\n\n");
            }

            objCHeaderFile.write(objCHeader.toString());

            objCHeaderFile.write("NSDictionary* get" + shaderType + "Dict(NSString* inShaderName) {\n");
            objCHeaderFile.write("    NSLog(@\"get" + shaderType + "Dict \");\n");
            for (String aShaderName : shaderFunctionNameList) {
                objCHeaderFile.write("    if ([inShaderName isEqualToString:@\"" + aShaderName + "\"]) {\n");
                objCHeaderFile.write("        NSLog(@\"get" + shaderType + "Dict() : calling -> get" + aShaderName + "_Uniform_VarID_Dict()\");\n");
                objCHeaderFile.write("        return get" + aShaderName + "_Uniform_VarID_Dict();\n");
                objCHeaderFile.write("    }\n");
            }

            objCHeaderFile.write("    return nil;\n");
            objCHeaderFile.write("};\n\n");

            objCHeaderFile.write("#endif\n");
            objCHeaderFile.close();
        } catch (IOException e) {
            System.out.println("An error occurred.");
            e.printStackTrace();
        }
    }

    @Override
    protected String getHeader() {
        StringBuilder header = new StringBuilder();

        header.append("#include \"" + FRAGMENT_SHADER_HEADER_FILE_NAME + "\"\n\n");

        uniformsForShaderFile = uniformsForShaderFile.replace(" float2", " vector_float2");
        uniformsForShaderFile = uniformsForShaderFile.replace(" float3", " vector_float3");
        uniformsForShaderFile = uniformsForShaderFile.replace(" float4", " vector_float4");
        header.append("typedef struct " + uniformStructName + " {\n" + uniformsForShaderFile + "} " + uniformStructName + ";\n\n");
        header.append("float4 " + sampleTexFuncName + "(sampler textureSampler, texture2d<float> colorTexture, float2 texCoord) {\n");
        header.append("    return colorTexture.sample(textureSampler, texCoord);\n");
        header.append("}\n\n");

        return header.toString();
    }

    @Override
    public String getShader() {
        String shader = super.getShader();
        updateCommonHeaders();
        String fragmentFunctionDef = "\n[[fragment]] float4 " + shaderFunctionName + "(VS_OUTPUT in [[ stage_in ]]";
        fragmentFunctionDef += ",\n    device " + uniformStructName + "& uniforms [[ buffer(0) ]]";
        fragmentFunctionDef += ",\n    sampler textureSampler [[sampler(0)]]";
        fragmentFunctionDef += ") {\n\nfloat4 outFragColor;";
        shader = shader.replace(MAIN, fragmentFunctionDef);

        int indexOfClosingBraceOfMain = shader.lastIndexOf('}');
        shader = shader.substring(0, indexOfClosingBraceOfMain) + "return outFragColor;\n\n" +
                shader.substring(indexOfClosingBraceOfMain, shader.length());

        for (String helperFunction : helperFunctions) {
            shader = shader.replaceAll("\\b" + helperFunction + "\\b", shaderFunctionName + "_" + helperFunction);
        }
        shader = shader.replaceAll("\\bsampleTex\\b", sampleTexFuncName);
        shader = shader.replaceAll("\\b" + sampleTexFuncName + "\\(uniforms" + "\\b" , sampleTexFuncName + "(textureSampler, uniforms");

        return shader;
    }

    public void setShaderNameAndHeaderPath(String name, String genMetalShaderPath) {
        // System.err.println("setShaderHeaderPath: " + genMetalShaderPath);
        shaderFunctionName = name;
        shaderFunctionNameList.add(shaderFunctionName);
        if (headerFilesDir == null) {
            headerFilesDir = genMetalShaderPath.substring(0, genMetalShaderPath.indexOf("jsl-"));
            headerFilesDir += MTL_HEADERS_DIR;
        }
        isPrismShader = genMetalShaderPath.contains("jsl-prism");
        resetVariables();
    }

    private void resetVariables() {
        uniformStructName = shaderFunctionName + "_Uniforms";
        uniformIDsEnumName = shaderFunctionName + "_ArgumentBufferID";
        textureSamplerName = shaderFunctionName + "_textureSampler";
        sampleTexFuncName = shaderFunctionName + "_SampleTexture";

        helperFunctions = new ArrayList<>();
        uniformNames = new ArrayList<>();
        uniformsForShaderFile = "";
        uniformsForObjCFiles = "";
        uniformIDs = "";
        uniformIDCount = 0;
        if (isPrismShader) {
            // System.err.println("Prism Shader: " + shaderFunctionName);
            objCHeaderFileName = PRISM_SHADER_HEADER_FILE_NAME;
        } else {
            // System.err.println("Decora Shader: " + shaderFunctionName);
            objCHeaderFileName = DECORA_SHADER_HEADER_FILE_NAME;
        }
    }
}
