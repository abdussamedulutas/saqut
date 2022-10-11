<?php

namespace App\Http\Controllers;

use Laravel\Lumen\Routing\Controller;
use Illuminate\Support\Facades\File;

class UserController extends Controller
{
    public function init()
    {
        $path = public_path() . "/info.html";
        return File::get($path);
    }
    public function home(\Illuminate\Http\Request $request)
    {
        return response()->json([
            "hep" => $request->all()
        ]);
    }
}
