#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace RawIronForestExport
{
    /// <summary>
    /// Batch export BOTD conifer meshes (Unity .asset) and prefabs to Wavefront OBJ for RawIron.
    /// Invoked via Unity -batchmode -executeMethod RawIronForestExport.ExportForRawIron.Run
    /// </summary>
    public static class ExportForRawIron
    {
        const string MeshFolder = "Assets/Assets/Conifers [BOTD]/Sources/Meshes";
        const string PrefabFolder = "Assets/Assets/Conifers [BOTD]/Prefabs";
        const string DefaultOutput =
            "../../../Games/WildernessRuins/Assets/Generated/ForestScene/Meshes";

        public static void Run()
        {
            var outputRoot = ResolveOutputRoot();
            Directory.CreateDirectory(outputRoot);

            var manifestLines = new List<string>
            {
                "{",
                "  \"sourceProject\": \"Forest Scene\",",
                "  \"unityVersion\": \"" + Application.unityVersion + "\",",
                "  \"exports\": ["
            };

            var exportCount = 0;

            exportCount += ExportMeshAssets(MeshFolder, outputRoot, manifestLines);
            exportCount += ExportPrefabMeshes(PrefabFolder, outputRoot, manifestLines);

            if (exportCount > 0 && manifestLines[manifestLines.Count - 1].EndsWith(","))
            {
                manifestLines[manifestLines.Count - 1] =
                    manifestLines[manifestLines.Count - 1].TrimEnd(',');
            }

            manifestLines.Add("  ]");
            manifestLines.Add("}");

            var manifestPath = Path.Combine(outputRoot, "export-manifest.json");
            File.WriteAllLines(manifestPath, manifestLines, Encoding.UTF8);

            Debug.Log("[RawIronForestExport] Wrote " + exportCount + " mesh file(s) to " + outputRoot);
            Debug.Log("[RawIronForestExport] Manifest: " + manifestPath);

            EditorApplication.Exit(exportCount > 0 ? 0 : 1);
        }

        static string ResolveOutputRoot()
        {
            var fromEnv = Environment.GetEnvironmentVariable("RAWIRON_FOREST_EXPORT_DIR");
            if (!string.IsNullOrWhiteSpace(fromEnv))
            {
                return Path.GetFullPath(fromEnv);
            }

            var projectRoot = Directory.GetParent(Application.dataPath).FullName;
            return Path.GetFullPath(Path.Combine(projectRoot, DefaultOutput));
        }

        static int ExportMeshAssets(string assetFolder, string outputRoot, List<string> manifestLines)
        {
            var count = 0;
            var guids = AssetDatabase.FindAssets("t:Mesh", new[] { assetFolder });
            foreach (var guid in guids)
            {
                var assetPath = AssetDatabase.GUIDToAssetPath(guid);
                var mesh = AssetDatabase.LoadAssetAtPath<Mesh>(assetPath);
                if (mesh == null)
                {
                    continue;
                }

                var fileName = SanitizeFileName(mesh.name) + ".obj";
                var objPath = Path.Combine(outputRoot, fileName);
                WriteObj(mesh, Matrix4x4.identity, objPath);
                AppendManifestEntry(manifestLines, mesh.name, assetPath, objPath, "mesh-asset");
                count++;
            }

            return count;
        }

        static int ExportPrefabMeshes(string prefabFolder, string outputRoot, List<string> manifestLines)
        {
            var count = 0;
            var guids = AssetDatabase.FindAssets("t:Prefab", new[] { prefabFolder });
            foreach (var guid in guids)
            {
                var assetPath = AssetDatabase.GUIDToAssetPath(guid);
                var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(assetPath);
                if (prefab == null)
                {
                    continue;
                }

                var instance = PrefabUtility.InstantiatePrefab(prefab) as GameObject;
                if (instance == null)
                {
                    continue;
                }

                try
                {
                    instance.transform.position = Vector3.zero;
                    instance.transform.rotation = Quaternion.identity;
                    instance.transform.localScale = Vector3.one;

                    var filters = instance.GetComponentsInChildren<MeshFilter>(true);
                    var meshIndex = 0;
                    foreach (var filter in filters)
                    {
                        if (filter.sharedMesh == null)
                        {
                            continue;
                        }

                        var baseName = SanitizeFileName(prefab.name);
                        if (filters.Length > 1)
                        {
                            baseName += "_" + SanitizeFileName(filter.sharedMesh.name);
                        }

                        if (meshIndex > 0)
                        {
                            baseName += "_" + meshIndex;
                        }

                        var objPath = Path.Combine(outputRoot, baseName + ".obj");
                        WriteObj(filter.sharedMesh, filter.transform.localToWorldMatrix, objPath);
                        AppendManifestEntry(manifestLines, prefab.name, assetPath, objPath, "prefab");
                        count++;
                        meshIndex++;
                    }
                }
                finally
                {
                    UnityEngine.Object.DestroyImmediate(instance);
                }
            }

            return count;
        }

        static void AppendManifestEntry(
            List<string> manifestLines,
            string label,
            string sourceAsset,
            string objPath,
            string kind)
        {
            if (manifestLines.Count > 4)
            {
                manifestLines[manifestLines.Count - 1] += ",";
            }

            manifestLines.Add("    {");
            manifestLines.Add("      \"label\": \"" + EscapeJson(label) + "\",");
            manifestLines.Add("      \"kind\": \"" + EscapeJson(kind) + "\",");
            manifestLines.Add("      \"source\": \"" + EscapeJson(sourceAsset) + "\",");
            manifestLines.Add("      \"obj\": \"" + EscapeJson(objPath.Replace('\\', '/')) + "\"");
            manifestLines.Add("    }");
        }

        static string EscapeJson(string value)
        {
            return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        static string SanitizeFileName(string value)
        {
            foreach (var invalid in Path.GetInvalidFileNameChars())
            {
                value = value.Replace(invalid, '_');
            }

            return value.Replace(' ', '_');
        }

        static void WriteObj(Mesh mesh, Matrix4x4 transform, string path)
        {
            var vertices = mesh.vertices;
            var normals = mesh.normals;
            var uvs = mesh.uv;
            var hasNormals = normals != null && normals.Length == vertices.Length;
            var hasUvs = uvs != null && uvs.Length == vertices.Length;

            var sb = new StringBuilder(vertices.Length * 32);
            sb.AppendLine("# RawIron Forest Scene export");
            sb.AppendLine("o " + mesh.name);

            for (var i = 0; i < vertices.Length; i++)
            {
                var world = transform.MultiplyPoint3x4(vertices[i]);
                sb.Append("v ");
                sb.Append(world.x.ToString("G9"));
                sb.Append(' ');
                sb.Append(world.y.ToString("G9"));
                sb.Append(' ');
                sb.AppendLine(world.z.ToString("G9"));
            }

            if (hasNormals)
            {
                for (var i = 0; i < normals.Length; i++)
                {
                    var worldNormal = transform.MultiplyVector(normals[i]).normalized;
                    sb.Append("vn ");
                    sb.Append(worldNormal.x.ToString("G9"));
                    sb.Append(' ');
                    sb.Append(worldNormal.y.ToString("G9"));
                    sb.Append(' ');
                    sb.AppendLine(worldNormal.z.ToString("G9"));
                }
            }

            if (hasUvs)
            {
                for (var i = 0; i < uvs.Length; i++)
                {
                    sb.Append("vt ");
                    sb.Append(uvs[i].x.ToString("G9"));
                    sb.Append(' ');
                    sb.AppendLine(uvs[i].y.ToString("G9"));
                }
            }

            for (var subMesh = 0; subMesh < mesh.subMeshCount; subMesh++)
            {
                var triangles = mesh.GetTriangles(subMesh);
                for (var t = 0; t < triangles.Length; t += 3)
                {
                    var i0 = triangles[t] + 1;
                    var i1 = triangles[t + 1] + 1;
                    var i2 = triangles[t + 2] + 1;
                    sb.Append("f ");
                    sb.Append(FormatFaceIndex(i0, hasUvs, hasNormals));
                    sb.Append(' ');
                    sb.Append(FormatFaceIndex(i1, hasUvs, hasNormals));
                    sb.Append(' ');
                    sb.AppendLine(FormatFaceIndex(i2, hasUvs, hasNormals));
                }
            }

            File.WriteAllText(path, sb.ToString(), Encoding.UTF8);
        }

        static string FormatFaceIndex(int index, bool hasUv, bool hasNormal)
        {
            if (hasUv && hasNormal)
            {
                return index + "/" + index + "/" + index;
            }

            if (hasUv)
            {
                return index + "/" + index;
            }

            if (hasNormal)
            {
                return index + "//" + index;
            }

            return index.ToString();
        }
    }
}
#endif
